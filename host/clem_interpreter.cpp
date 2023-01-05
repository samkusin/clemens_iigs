#include "clem_interpreter.hpp"
#include "clem_backend.hpp"

#include <charconv>

namespace {

std::string_view trimLeft(const std::string_view &token, size_t off = 0,
                          size_t length = std::string_view::npos) {
    auto tmp = token.substr(off, length);
    auto left = tmp.begin();
    for (; left != tmp.end(); ++left) {
        if (!std::isspace(*left))
            break;
    }
    return tmp.substr(left - tmp.begin());
}

std::string_view trimToken(const std::string_view &token, size_t off = 0,
                           size_t length = std::string_view::npos) {
    auto tmp = trimLeft(token, off, length);
    auto right = tmp.rbegin();
    for (; right != tmp.rend(); ++right) {
        if (!std::isspace(*right))
            break;
    }
    tmp = tmp.substr(0, tmp.size() - (right - tmp.rbegin()));
    return tmp;
}

bool expect(std::string_view &result, std::string_view token) {
    auto tmp = trimLeft(result);
    if (tmp.compare(0, token.size(), token) != 0)
        return false;
    result = tmp.substr(token.size());
    return true;
}

std::string_view extractHex(std::string_view &script) {
    std::string_view result;
    auto tmp = trimLeft(script);

    if (!tmp.empty()) {
        auto it = tmp.begin();
        for (; it != tmp.end(); ++it) {
            if (std::isdigit(*it))
                continue;
            if ((*it >= 'A' && *it <= 'F') || (*it >= 'a' && *it <= 'f'))
                continue;
            break;
        }
        result = tmp.substr(0, it - tmp.begin());
    }
    if (!result.empty()) {
        script = tmp.substr(result.size());
    }
    return result;
}

std::string_view extractInt(std::string_view &script) {
    std::string_view result;
    auto tmp = trimLeft(script);

    if (!tmp.empty()) {
        auto itBegin = tmp.begin();
        auto it = itBegin;
        bool hasValue = false;
        if (*it == '-' || *it == '+')
            ++it;
        if (*it == '+')
            ++itBegin;
        for (; it != tmp.end(); ++it) {
            if (std::isdigit(*it)) {
                hasValue = true;
                continue;
            }
            break;
        }
        // if our string starts with "+/-" there must be a digit afterwards
        if (hasValue) {
            result = tmp.substr(0, it - itBegin);
        }
    }
    if (!result.empty()) {
        script = tmp.substr(result.size());
    }
    return result;
}

} // namespace

/*  Language:

    word := HEX32
    number := UINT32
    number_operand := '#' number      (decimal number)
                   | '#$' HEX32         (hex number)
                   | '0x' HEX32
    identifier := '.' ('A' | 'X' | 'Y' | 'PC' | 'PBR' | 'DBR' | 'D' | 'SP')
    memory_address := '$' word
                   | '$' HEX8 '/' HEX16

    expression := number
               | number_operand
               | string_literal
               | identifier
               | memory_address
               | '(' command ')'

    memory_value_expr := word
                      | number_operand
                      | identifier
                      | memory_address

    memory_address_lefthand := memory_address

    assignment := identifier (':'|'=') expression
               | memory_address_lefthand (':'|'=') memory_value_expr

    expression_list := expression (',' expression_list)

    command := action SPC expression_list

    statement := command
              | expression
              | assignment

    statement_list := statement (';' statement_list)

    Uses a top-down recursive parsing approach.

    v1:
      expression := number_operand
      assignment := identifier (':'|'=') expression
      statement := assignment
      statement_list := statement (';' statement_list)

*/

struct ClemensInterpreter::ASTNode {
    //  NULL is the root of an AST node
    ASTNode *parent;
    // To keep singly-linked, child->sibling is the first node
    ASTNode *child;
    // Tne parent uses child->sibling to traverse a child list (see above.)
    // Siblings can use this to access its next sibling.
    ASTNode *sibling;
    //  Identifies the node's function in the AST
    ASTNodeType type;
    // Links to a copy of a token from the script (memory owned by the slab)
    std::string_view token;
};

ClemensInterpreter::ClemensInterpreter(cinek::FixedStack slab)
    : slab_(std::move(slab)), ast_(nullptr), astFreed_(nullptr) {
    machineProperties_["a"] = ClemensBackendMachineProperty::RegA;
    machineProperties_["A"] = ClemensBackendMachineProperty::RegA;
    machineProperties_["b"] = ClemensBackendMachineProperty::RegB;
    machineProperties_["B"] = ClemensBackendMachineProperty::RegB;
    machineProperties_["c"] = ClemensBackendMachineProperty::RegC;
    machineProperties_["C"] = ClemensBackendMachineProperty::RegC;
    machineProperties_["x"] = ClemensBackendMachineProperty::RegX;
    machineProperties_["X"] = ClemensBackendMachineProperty::RegX;
    machineProperties_["y"] = ClemensBackendMachineProperty::RegY;
    machineProperties_["Y"] = ClemensBackendMachineProperty::RegY;
    machineProperties_["d"] = ClemensBackendMachineProperty::RegD;
    machineProperties_["D"] = ClemensBackendMachineProperty::RegD;
    machineProperties_["p"] = ClemensBackendMachineProperty::RegP;
    machineProperties_["P"] = ClemensBackendMachineProperty::RegP;
    machineProperties_["s"] = ClemensBackendMachineProperty::RegSP;
    machineProperties_["S"] = ClemensBackendMachineProperty::RegSP;
    machineProperties_["dbr"] = ClemensBackendMachineProperty::RegDBR;
    machineProperties_["DBR"] = ClemensBackendMachineProperty::RegDBR;
    machineProperties_["pbr"] = ClemensBackendMachineProperty::RegPBR;
    machineProperties_["PBR"] = ClemensBackendMachineProperty::RegPBR;
    machineProperties_["pc"] = ClemensBackendMachineProperty::RegPC;
    machineProperties_["PC"] = ClemensBackendMachineProperty::RegPC;

    ast_ = createASTNode(ASTNodeType::Root);
}

auto ClemensInterpreter::addASTNodeToSibling(ASTNode *node, ASTNode *prevSibling) -> ASTNode * {
    ASTNode *nextSibling = prevSibling->sibling;
    prevSibling->sibling = node->sibling;
    node->sibling = nextSibling;
    return node;
}

auto ClemensInterpreter::addASTNodeToParent(ASTNode *node, ASTNode *parent) -> ASTNode * {
    if (!parent->child) {
        parent->child = node;
    }
    node = addASTNodeToSibling(node, parent->child);
    node->parent = parent;
    return node;
}

auto ClemensInterpreter::createASTNode(ASTNodeType type, ASTNode *parent) -> ASTNode * {
    auto *node = astFreed_;
    if (node) {
        node = astFreed_->sibling;
        if (node == astFreed_->sibling) {
            astFreed_ = nullptr;
        } else {
            astFreed_->sibling = node->sibling;
        }
        node->~ASTNode();
        ::new (node) ASTNode();
    } else {
        node = slab_.newItem<ASTNode>();
        if (!node)
            return nullptr;
    }
    node->type = type;
    node->sibling = node;
    if (parent) {
        node = addASTNodeToParent(node, parent);
    }
    return node;
}

auto ClemensInterpreter::destroyASTNode(ASTNode *node) -> ASTNode * {
    ASTNode *prevChild = node->child;
    if (prevChild) {
        ASTNode *child = prevChild->sibling;
        while (child != prevChild) {
            prevChild->sibling = child->sibling;
            child = destroyASTNode(child);
        }
        destroyASTNode(child);
        node->child = nullptr;
    }
    //  sibling detach does not occur here but by the parent
    ASTNode *sibling = node->sibling;
    node->child = nullptr;
    node->sibling = astFreed_ ? astFreed_->sibling : node;
    astFreed_->sibling = node;
    astFreed_ = node;
    return sibling;
}

std::string_view ClemensInterpreter::allocate(std::string_view token) {
    std::string_view result;
    token = trimToken(token);
    if (!token.empty()) {
        char *data = (char *)slab_.allocate(token.size());
        token.copy(data, token.size());
        result = std::string_view(data, token.size());
    }
    return result;
}

auto ClemensInterpreter::parseNumber(std::string_view script) -> ParseResult {
    //  accepts both digits and hex digits and lets the interpreter decide on
    //  how to treat the operand.
    ParseResult number(script);
    auto token = extractHex(script);
    if (token.empty()) {
        //  possibly a number with a +- symbol at the start
        token = extractInt(script);
    }
    if (token.empty()) {
        return number.fail(script);
    }
    if (token[0] == '-') {
        //  enforce integer number type
        number.node = createASTNode(ASTNodeType::IntegerValue);
    } else {
        number.node = createASTNode(ASTNodeType::AnyIntegerValue);
    }
    number.node->token = allocate(token);
    return number.accept(script);
}

auto ClemensInterpreter::parseHexNumber(std::string_view script) -> ParseResult {
    auto number = parseNumber(script);
    if (!number || number.node->type == ASTNodeType::IntegerValue) {
        return number.fail();
    }
    number.node->type = ASTNodeType::HexIntegerValue;
    return number.accept(number.script());
}

auto ClemensInterpreter::parseDecimalNumber(std::string_view script) -> ParseResult {
    ParseResult number(script);
    auto token = extractInt(script);
    if (token.empty()) {
        return number.fail(script);
    }
    number.node = createASTNode(ASTNodeType::IntegerValue);
    number.node->token = allocate(token);
    return number.accept(script);
}

auto ClemensInterpreter::parseNumberOperand(std::string_view script) -> ParseResult {
    //  input comforms
    //      where digit = [0-9] and digithex = [0-9|a-f|A-f] then:
    //        [digithex](digithex)* |
    //        [#][digit](digit)* |
    //        [#][$][digithex](digithex)*
    ParseResult number(script);
    auto input = trimLeft(script);
    if (expect(input, "#")) {
        if (expect(input, "$")) {
            //  hex only
            number = parseHexNumber(input);
        } else {
            //  decimal only
            number = parseDecimalNumber(input);
        }
    } else {
        number = parseNumber(input);
    }

    return number;
}

auto ClemensInterpreter::parseIdentifier(std::string_view script) -> ParseResult {
    // input conforms to .A-Za-z_(A-Za-z_0-9)*
    ParseResult result(script);
    auto input = trimLeft(script);
    if (!expect(input, ".")) {
        return result;
    }
    auto it = input.begin();
    if (it == input.end()) {
        return result;
    }
    if (!std::isalpha(*it) || *it == '_') {
        return result;
    }
    for (++it; it != input.end(); ++it) {
        auto chidx = it - input.begin();
        if (std::isalnum(*it))
            continue;
        if (*it == '_')
            continue;
        auto identifier = input.substr(0, chidx);
        result.accept(input.substr(chidx));
        result.node = createASTNode(ASTNodeType::Identifier);
        result.node->token = allocate(identifier);
        break;
    }
    return result;
}

auto ClemensInterpreter::parseExpression(std::string_view script) -> ParseResult {
    //  expression := number_operand
    return parseNumberOperand(script);
}

auto ClemensInterpreter::parseAssignment(std::string_view script) -> ParseResult {
    //  assignment := identifier (':'|'=') expression
    ParseResult identifier = parseIdentifier(script);
    if (!identifier.ok()) {
        return identifier.revert(script);
    }
    auto righthand = identifier.script();
    if (!expect(righthand, "=") && !expect(righthand, ":")) {
        // destroyASTNode(identifier.node);
        return identifier.revert(script);
    }
    ParseResult expression = parseExpression(righthand);
    if (!expression.ok()) {
        // destroyASTNode(identifier.node);
        return expression.revert(script);
    }

    ParseResult assignment(Result{Result::Ok, expression.script()});
    assignment.node = createASTNode(ASTNodeType::Assignment);
    addASTNodeToParent(identifier.node, assignment.node);
    addASTNodeToParent(expression.node, assignment.node);
    return assignment;
}

auto ClemensInterpreter::parseStatement(std::string_view script) -> ParseResult {
    auto assignment = parseAssignment(script);
    if (!assignment.ok()) {
        return assignment.revert(script);
    }
    return assignment;
}

auto ClemensInterpreter::parseStatementList(std::string_view script) -> ParseResult {
    auto statement = parseStatement(script);
    if (!statement.ok()) {
        return statement.revert(script);
    }
    //  optional extra statements
    auto righthand = statement.script();
    if (!expect(righthand, ";")) {
        auto remainder = trimLeft(statement.script());
        return !remainder.empty() ? statement.fail(remainder) : statement;
    }
    auto statementList = parseStatementList(righthand);
    if (!statementList.ok()) {
        return statementList.revert(righthand);
    }
    statementList.node = addASTNodeToSibling(statementList.node, statement.node);
    return statementList;
}

auto ClemensInterpreter::parse(std::string_view script) -> Result {
    auto statementList = parseStatementList(script);
    if (statementList.ok()) {
        auto *chain = createASTNode(ASTNodeType::Chain, ast_);
        addASTNodeToParent(statementList.node, chain);
    }
    return statementList.result;
}

void ClemensInterpreter::execute(ClemensBackend *backend) {
    execute(ast_, backend);
    slab_.reset();
    astFreed_ = nullptr;
    ast_ = createASTNode(ASTNodeType::Root);
}

bool ClemensInterpreter::execute(ASTNode *node, ClemensBackend *backend) {
    ASTNode *child = node->child ? node->child->sibling : nullptr;
    switch (node->type) {
    case ASTNodeType::Root:
    case ASTNodeType::Chain:
        if (child) {
            do {
                if (!execute(child, backend))
                    return false;
                child = child->sibling;
            } while (child != node->child);
        }
        break;
    case ASTNodeType::Assignment:
        //  left-hand = target
        //  right-hand = origin (value or identifier)
        //             = integer values are by default hex (with #decimal supported)
        if (child) {
            ASTNode *left = child->sibling;
            ASTNode *right = child;
            uint32_t u32;
            if (!execute(left, backend))
                return false;
            if (left->type != ASTNodeType::Identifier)
                return false;
            if (!execute(right, backend))
                return false;
            if (right->type == ASTNodeType::IntegerValue) {
                int i32;
                if (std::from_chars(right->token.data(), right->token.data() + right->token.size(),
                                    i32, 10)
                        .ec != std::errc{}) {
                    return false;
                }
                u32 = (uint32_t)i32;
            } else {
                if (std::from_chars(right->token.data(), right->token.data() + right->token.size(),
                                    u32, 16)
                        .ec != std::errc{}) {
                    return false;
                }
            }
            auto machinePropertyIt = machineProperties_.find(left->token);
            if (machinePropertyIt != machineProperties_.end()) {
                backend->assignPropertyToU32(machinePropertyIt->second, u32);
            } else {
                return false;
            }
        }
        break;
    case ASTNodeType::Identifier:
    case ASTNodeType::AnyIntegerValue:
    case ASTNodeType::HexIntegerValue:
    case ASTNodeType::IntegerValue:
        break;
    }
    return true;
}
