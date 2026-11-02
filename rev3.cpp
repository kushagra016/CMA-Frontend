#include <iostream>
#include <cstddef>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

bool suppressOutput = true; 

// Hardware Alignment Helper
size_t alignToWordBoundary(size_t bytes, size_t alignment = 8) {
    return (bytes + alignment - 1) & ~(alignment - 1);
}

struct BlockHeader {
    int uniqueId; // NEW: Bulletproof ID tracking
    size_t size;
    size_t requestedSize;
    bool isFree;
    BlockHeader *next;
    BlockHeader *prev;
};

enum class AllocationStrategy { FIRST_FIT, BEST_FIT, WORST_FIT };

size_t convertToBytes(double value, std::string unit) {
    if (unit == "KB") return static_cast<size_t>(value * 1024);
    if (unit == "MB") return static_cast<size_t>(value * 1024 * 1024);
    if (unit == "GB") return static_cast<size_t>(value * 1024 * 1024 * 1024);
    return static_cast<size_t>(value);
}

double convertFromBytes(size_t bytes, std::string unit) {
    if (unit == "KB") return static_cast<double>(bytes) / 1024.0;
    if (unit == "MB") return static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (unit == "GB") return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return static_cast<double>(bytes);
}

struct AllocationRecord {
    void* firstFitPtr;
    void* bestFitPtr;
    void* worstFitPtr;
};
std::vector<AllocationRecord> allocatedPointers;

class MemoryAllocator {
private:
    void *memoryPool;
    size_t totalSize;
    BlockHeader *head;
    BlockHeader *firstFreeBlock; 
    std::string strategyName;
    std::mutex arenaMutex;

public:
    MemoryAllocator(size_t size, std::string name) {
        totalSize = size;
        strategyName = name;
        memoryPool = ::operator new(totalSize);
        head = static_cast<BlockHeader *>(memoryPool);

        head->uniqueId = 0; 
        head->size = totalSize - sizeof(BlockHeader);
        head->requestedSize = 0; 
        head->isFree = true;
        head->next = nullptr;
        head->prev = nullptr;

        firstFreeBlock = head; 
    }

    ~MemoryAllocator() {
        ::operator delete(memoryPool);
    }

    void *allocate(size_t rawSize, AllocationStrategy strategy, int newId) {
        std::lock_guard<std::mutex> lock(arenaMutex); 
        
        if (rawSize == 0) return nullptr;

        size_t alignedSize = alignToWordBoundary(rawSize);
        BlockHeader *current = (strategy == AllocationStrategy::FIRST_FIT) ? firstFreeBlock : head; 
        BlockHeader *selectedBlock = nullptr;

        if (strategy == AllocationStrategy::FIRST_FIT) {
            while (current != nullptr) {
                if (current->isFree && current->size >= alignedSize) {
                    selectedBlock = current;
                    break;
                }
                current = current->next;
            }
        }
        else if (strategy == AllocationStrategy::BEST_FIT) {
            while (current != nullptr) {
                if (current->isFree && current->size >= alignedSize) {
                    if (selectedBlock == nullptr || current->size < selectedBlock->size) {
                        selectedBlock = current;
                    }
                }
                current = current->next;
            }
        }
        else if (strategy == AllocationStrategy::WORST_FIT) {
            while (current != nullptr) {
                if (current->isFree && current->size >= alignedSize) {
                    if (selectedBlock == nullptr || current->size > selectedBlock->size) {
                        selectedBlock = current;
                    }
                }
                current = current->next;
            }
        }

        if (selectedBlock == nullptr) return nullptr;

        if (selectedBlock->size >= alignedSize + sizeof(BlockHeader) + 1) {
            char *newBlockAddress = reinterpret_cast<char *>(selectedBlock) + sizeof(BlockHeader) + alignedSize;
            BlockHeader *newBlock = reinterpret_cast<BlockHeader *>(newBlockAddress);

            newBlock->size = selectedBlock->size - alignedSize - sizeof(BlockHeader);
            newBlock->requestedSize = 0;
            newBlock->uniqueId = 0;
            newBlock->isFree = true;
            newBlock->next = selectedBlock->next;
            newBlock->prev = selectedBlock;

            if (newBlock->next != nullptr) newBlock->next->prev = newBlock;

            selectedBlock->size = alignedSize;
            selectedBlock->next = newBlock;
        }

        selectedBlock->isFree = false;
        selectedBlock->requestedSize = rawSize; 
        selectedBlock->uniqueId = newId; // Assign exact ID

        if (selectedBlock == firstFreeBlock) {          
            BlockHeader* temp = firstFreeBlock->next;
            while (temp != nullptr && !temp->isFree) temp = temp->next;
            firstFreeBlock = temp;
        }
        
        return static_cast<void *>(selectedBlock + 1);
    }

    void deallocate(void *ptr) {
        if (ptr == nullptr) return;

        std::lock_guard<std::mutex> lock(arenaMutex); 
        BlockHeader *current = static_cast<BlockHeader *>(ptr) - 1;
        
        current->isFree = true;
        current->requestedSize = 0;
        current->uniqueId = 0; // Clear ID on free

        if (current->next != nullptr && current->next->isFree) {
            current->size += sizeof(BlockHeader) + current->next->size;
            current->next = current->next->next;
            if (current->next != nullptr) current->next->prev = current;
        }

        if (current->prev != nullptr && current->prev->isFree) {
            current->prev->size += sizeof(BlockHeader) + current->size;
            current->prev->next = current->next;
            if (current->next != nullptr) current->next->prev = current->prev;
            current = current->prev; 
        }

        if (firstFreeBlock == nullptr || current < firstFreeBlock) {   
            firstFreeBlock = current;
        }
    }
    
    void* getPointerByUniqueId(int targetId) {
        std::lock_guard<std::mutex> lock(arenaMutex);
        BlockHeader* current = head;
        while (current != nullptr) {
            if (current->uniqueId == targetId && !current->isFree) {
                return static_cast<void*>(current + 1);
            }
            current = current->next;
        }
        return nullptr;
    }

    json getMemoryMapJson(const std::string &unit) {
        std::lock_guard<std::mutex> lock(arenaMutex);
        BlockHeader *current = head;
        json mapArray = json::array();

        while (current != nullptr) {
            json block;
            block["id"] = current->uniqueId; 
            block["status"] = current->isFree ? "FREE" : "USED";
            block["sizeBytes"] = current->size;
            block["displaySize"] = convertFromBytes(current->size, unit);
            block["unit"] = unit;
            mapArray.push_back(block);
            current = current->next;
        }
        return mapArray;
    }

    json getAnalyticsJson() {
        std::lock_guard<std::mutex> lock(arenaMutex);
        BlockHeader *current = head;
        size_t totalFreeBytes = 0, totalUsedBytes = 0, largestFreeBlock = 0;
        int blockCount = 0;

        while (current != nullptr) {
            if (current->isFree) {
                totalFreeBytes += current->size;
                if (current->size > largestFreeBlock) largestFreeBlock = current->size;
            } else {
                totalUsedBytes += current->size;
            }
            blockCount++;
            current = current->next;
        }

        double externalFrag = (totalFreeBytes > 0) ? (static_cast<double>(totalFreeBytes - largestFreeBlock) / totalFreeBytes) * 100.0 : 0.0;
        json stats;
        stats["externalFrag"] = externalFrag;
        stats["totalBlocks"] = blockCount;
        return stats;
    }
};

MemoryAllocator* globalAllocator = nullptr;
AllocationStrategy globalStrategy = AllocationStrategy::FIRST_FIT;
std::mutex webRoutingMutex; 
int globalAllocationCounter = 1;

int main() {
    std::cout << "Initializing Stabilized Server on http://localhost:8080...\n";
    
    size_t poolSize = 10 * 1024 * 1024; // 10MB Pool
    MemoryAllocator allocFirst(poolSize, "FIRST-FIT STRATEGY");
    MemoryAllocator allocBest(poolSize, "BEST-FIT STRATEGY");
    MemoryAllocator allocWorst(poolSize, "WORST-FIT STRATEGY");

    httplib::Server svr;

    svr.Options(".*", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    svr.Get("/api/memory-map", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); 
        
        // Read the requested unit from the query parameter (e.g., ?unit=MB)
        // Default to KB if the parameter is missing
        std::string requestedUnit = "KB";
        if (req.has_param("unit")) {
            requestedUnit = req.get_param_value("unit");
        }
        
        json response;
        response["firstFit"] = allocFirst.getMemoryMapJson(requestedUnit);
        response["bestFit"] = allocBest.getMemoryMapJson(requestedUnit);
        response["worstFit"] = allocWorst.getMemoryMapJson(requestedUnit);
        
        res.set_content(response.dump(), "application/json");
    });

    svr.Get("/api/analytics", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); 
        json response;
        response["firstFit"] = allocFirst.getAnalyticsJson();
        response["bestFit"] = allocBest.getAnalyticsJson();
        response["worstFit"] = allocWorst.getAnalyticsJson();
        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/allocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);
        
        double val = reqData["val"];
        std::string unit = reqData["unit"];
        size_t requestedBytes = convertToBytes(val, unit);
        
        void *ptrF = nullptr, *ptrB = nullptr, *ptrW = nullptr;

        {
            std::lock_guard<std::mutex> routeLock(webRoutingMutex);
            int currentId = globalAllocationCounter++; 
            
            ptrF = allocFirst.allocate(requestedBytes, AllocationStrategy::FIRST_FIT, currentId);
            ptrB = allocBest.allocate(requestedBytes, AllocationStrategy::BEST_FIT, currentId);
            ptrW = allocWorst.allocate(requestedBytes, AllocationStrategy::WORST_FIT, currentId);
        }
        
        json response;
        if (ptrF && ptrB && ptrW) {
            allocatedPointers.push_back({ptrF, ptrB, ptrW});
            response["status"] = "SUCCESS";
        } else {
            std::lock_guard<std::mutex> routeLock(webRoutingMutex);
            if (ptrF) allocFirst.deallocate(ptrF);
            if (ptrB) allocBest.deallocate(ptrB);
            if (ptrW) allocWorst.deallocate(ptrW);
            response["status"] = "FAILED";
        }
        res.set_content(response.dump(), "application/json");
    });

    svr.Post("/api/deallocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);
        int targetId = reqData["id"]; 
        
        json response;
        response["status"] = "FAILED";

        void* primaryPtr = allocFirst.getPointerByUniqueId(targetId);

        if (primaryPtr) {
            auto it = std::find_if(allocatedPointers.begin(), allocatedPointers.end(),
                [&](const AllocationRecord& rec) { return rec.firstFitPtr == primaryPtr; });

            if (it != allocatedPointers.end()) {
                std::lock_guard<std::mutex> routeLock(webRoutingMutex);
                allocFirst.deallocate(it->firstFitPtr);
                allocBest.deallocate(it->bestFitPtr);
                allocWorst.deallocate(it->worstFitPtr);
                
                allocatedPointers.erase(it);
                response["status"] = "SUCCESS";
            }
        }
        res.set_content(response.dump(), "application/json");
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}