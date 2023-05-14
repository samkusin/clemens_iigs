#ifndef CLEM_HOST_ASSET_BROWSER_HPP
#define CLEM_HOST_ASSET_BROWSER_HPP

#include "clem_file_browser.hpp"
#include "core/clem_disk_asset.hpp"

#include <filesystem>

class ClemensAssetBrowser : public ClemensFileBrowser {

  public:
    ClemensAssetBrowser();

    void setDiskType(ClemensDiskAsset::DiskType diskType);

    bool isSelectedFilePathNewFile() const {
        return createDiskImageType_ != ClemensDiskAsset::ImageNone;
    }

  protected:
    bool onCreateRecord(const std::filesystem::directory_entry &direntry,
                        ClemensFileBrowser::Record &) final;
    std::string onDisplayRecord(const ClemensFileBrowser::Record &record) final;
    BrowserFinishedStatus onExtraSelectionUI(ImVec2 dimensions, Record &selectedRecord) final;

  private:
    ClemensDiskAsset::DiskType diskType_;

    //  This should fit inside ClemensFileBrowser::Record::context and
    //  compilation will fail if it doesn't
    struct ClemensAssetData {
        ClemensDiskAsset::DiskType diskType;
        ClemensDiskAsset::ImageType imageType;
    };

    //  used for creating asset files
    char createDiskFilename_[128];
    int createDiskMBCount_;
    ClemensDiskAsset::ImageType createDiskImageType_;
};

#endif
