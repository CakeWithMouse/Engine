#pragma once
#include <Engine/SaveGame/SaveArchive.h>
#include <Engine/Components/ComponentType.h>
#include <filesystem>
#include <thread>

struct SaveGameResult {
    enum class Status { Idle, Accepted, Saved, Loaded, Failed, Busy } status=Status::Idle;
    uint64_t ticket=0;
    std::string message;
    bool Succeeded() const { return status==Status::Saved || status==Status::Loaded; }
};
// Owned by one Game, used only on its owner thread. No C++ object crosses the module ABI.
class SaveGame {
public:
    explicit SaveGame(Game& game);
    // Catalog is supplied automatically by scene modules/editor Play. Standalone games set it explicitly.
    void SetCatalog(ComponentTypeCatalog catalog) { catalog_=catalog; }
    ComponentTypeCatalog Catalog() const { return catalog_; }
    void Configure(std::string gameId, std::filesystem::path root = {});
    SaveGameResult RequestSave(const std::string& slot);
    SaveGameResult RequestLoad(const std::string& slot);
    const SaveGameResult& LastResult() const { return result_; }
    std::vector<std::string> ListSlots() const;
    void DeleteSlot(const std::string& slot);
    // Explicit host/headless boundary; refuses to run during scene traversal.
    void ProcessPending();
    std::string Capture();
    void Restore(const std::string& bytes);
    std::string ReadSlot(const std::string& slot) const;
    void WriteSlot(const std::string& slot, const std::string& bytes);
private:
    void CheckThread() const;
    std::filesystem::path SlotPath(const std::string&) const;
    SaveGameResult Request(const std::string&, bool load);
    Game& game_; ComponentTypeCatalog catalog_{};
    std::string gameId_, pendingSlot_; std::filesystem::path root_;
    bool pending_=false, load_=false, processing_=false;
    uint64_t serial_=0; SaveGameResult result_;
    std::thread::id thread_=std::this_thread::get_id();
};
