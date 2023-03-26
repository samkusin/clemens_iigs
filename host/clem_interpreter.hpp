#ifndef CLEM_HOST_INTERPRETER_HPP
#define CLEM_HOST_INTERPRETER_HPP

#include "clem_host_shared.hpp"

#include "cinek/buffer.hpp"
#include "cinek/fixedstack.hpp"

#include <string_view>
#include <unordered_map>

class ClemensBackend;

class ClemensInterpreter {
  public:
    struct Result {
        enum { Ok, NoMatch, SyntaxError, Undefined } type;
        std::string_view script;
    };

  public:
    //  Construct an interpreter with slab memory for the AST
    ClemensInterpreter(cinek::FixedStack slab);

    //  Builds the AST for this script
    Result parse(std::string_view expression);
    void execute(ClemensBackend *backend);

  private:
    struct ASTNode;
    enum class ASTNodeType {
        Root,
        // list of actions
        Chain,
        // assignment(identifier, value)
        Assignment,
        // command
        Command,
        // identifies a variable or attribute
        Identifier,
        //  always regard as a decimal value
        IntegerValue,
        //  always regard as an integer
        HexIntegerValue,
        //  depends on the context
        AnyIntegerValue
    };

    struct ParseResult {
        ASTNode *node;
        Result result;

        ParseResult() : node(nullptr), result{Result::Undefined} {}
        ParseResult(std::string_view script)
            : node(nullptr), result(Result{Result::NoMatch, script}) {}
        ParseResult(Result res) : node(nullptr), result{res} {}

        operator bool() const { return ok(); }

        bool error() const { return result.type != Result::Ok; }
        bool nomatch() const { return result.type == Result::NoMatch; }
        bool ok() const { return result.type == Result::Ok; }
        std::string_view script() const { return result.script; }
        ParseResult &accept(std::string_view script) {
            result.type = Result::Ok;
            result.script = script;
            return *this;
        }
        ParseResult &revert(std::string_view old) {
            if (nomatch()) {
                result.script = old;
                result.type = Result::NoMatch;
            }
            return *this;
        }
        ParseResult &fail(std::string_view script = std::string_view()) {
            if (!script.empty()) {
                result.script = script;
            }
            result.type = Result::SyntaxError;
            return *this;
        }
    };

    //  creates an AST Node and adds it to an optional parent
    ASTNode *createASTNode(ASTNodeType type, ASTNode *parent = nullptr);
    ASTNode *addASTNodeToParent(ASTNode *node, ASTNode *parent);
    ASTNode *addASTNodeToSibling(ASTNode *node, ASTNode *sibling);
    ASTNode *destroyASTNode(ASTNode *node);
    std::string_view allocate(std::string_view token);

    bool execute(ASTNode *node, ClemensBackend *backend);

    ParseResult parseStatementList(std::string_view script);
    ParseResult parseStatement(std::string_view script);
    ParseResult parseAssignment(std::string_view script);
    ParseResult parseExpression(std::string_view script);
    ParseResult parseCommand(std::string_view script);
    ParseResult parseNumberOperand(std::string_view script);
    ParseResult parseIdentifier(std::string_view script);
    ParseResult parseNumber(std::string_view script);
    ParseResult parseHexNumber(std::string_view script);
    ParseResult parseDecimalNumber(std::string_view script);

  private:
    std::unordered_map<std::string_view, ClemensBackendMachineProperty> machineProperties_;

    cinek::FixedStack slab_;
    ASTNode *ast_;
    ASTNode *astFreed_;
};

#endif
