#include "clem_l10n.hpp"

namespace ClemensL10N {

#include "strings/clem_hotkeys.inl"
#include "strings/clem_welcome.inl"

const char *kExitMessage[] = {"Any unsaved progress will be lost.\nDo you want to exit?"};
const char *kViewInput[] = {"Move mouse into view for key input"};
const char *kModalDeleteSnapshot[] = {"Delete Snapshot"};
const char *kLabelDeleteConfirm[] = {"Are you sure you want to delete %s?"};
const char *kLabelDeleteFailed[] = {"Unable to delete the selected file."};
const char *kLabelDelete[] = {"Delete"};
const char *kLabelOk[] = {"Ok"};
const char *kLabelCancel[] = {"Cancel"};

#include "strings/clem_help.inl"
#include "strings/clem_settings.inl"

} // namespace ClemensL10N
