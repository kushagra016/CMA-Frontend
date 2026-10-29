#include <cstdlib> // Required to read Environment Variables

#include <iostream>
#include <cstddef>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "httplib.h"
#include "json.hpp"
using json = nlohmann::json;

// BlockHeader
struct BlockHeader
{
    size_t size;
    bool isFree;
    BlockHeader *next;
    BlockHeader *prev;
};

enum class AllocationStrategy
{
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT
};

size_t convertToBytes(double value, std::string unit)
{
    if (unit == "KB")
        return static_cast<size_t>(value * 1024);
    if (unit == "MB")
        return static_cast<size_t>(value * 1024 * 1024);
    if (unit == "GB")
        return static_cast<size_t>(value * 1024 * 1024 * 1024);
    return static_cast<size_t>(value);
}

double convertFromBytes(size_t bytes, std::string unit)
{
    if (unit == "KB")
        return static_cast<double>(bytes) / 1024.0;
    if (unit == "MB")
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (unit == "GB")
        return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return static_cast<double>(bytes);
}

struct AllocationRecord {
    void* firstFitPtr;
    void* bestFitPtr;
    void* worstFitPtr;
};
std::vector<AllocationRecord> allocatedPointers;

class MemoryAllocator
{
private:
    void *memoryPool;
    size_t totalSize;
    BlockHeader *head;
    std::string strategyName;

public:
    MemoryAllocator(size_t size, std::string name)
    {
        totalSize = size;
        strategyName = name;
        memoryPool = ::operator new(totalSize);
        head = static_cast<BlockHeader *>(memoryPool);

        head->size = totalSize - sizeof(BlockHeader);
        head->isFree = true;
        head->next = nullptr;
        head->prev = nullptr;
    }

    ~MemoryAllocator()
    {
        ::operator delete(memoryPool);
    }

    void *allocate(size_t size, AllocationStrategy strategy)
    {
        if (size == 0)
            return nullptr;

        BlockHeader *current = head;
        BlockHeader *selectedBlock = nullptr;

        // Searching Logic
        if (strategy == AllocationStrategy::FIRST_FIT)
        {
            while (current != nullptr)
            {
                if (current->isFree && current->size >= size)
                {
                    selectedBlock = current;
                    break;
                }
                current = current->next;
            }
        }
        else if (strategy == AllocationStrategy::BEST_FIT)
        {
            while (current != nullptr)
            {
                if (current->isFree && current->size >= size)
                {
                    if (selectedBlock == nullptr || current->size < selectedBlock->size)
                    {
                        selectedBlock = current;
                    }
                }
                current = current->next;
            }
        }
        else if (strategy == AllocationStrategy::WORST_FIT)
        {
            while (current != nullptr)
            {
                if (current->isFree && current->size >= size)
                {
                    if (selectedBlock == nullptr || current->size > selectedBlock->size)
                    {
                        selectedBlock = current;
                    }
                }
                current = current->next;
            }
        }

        if (selectedBlock == nullptr)
        {
            return nullptr;
        }

        // Merging & Spliting Logic
        if (selectedBlock->size >= size + sizeof(BlockHeader) + 1)
        {
            char *newBlockAddress = reinterpret_cast<char *>(selectedBlock) + sizeof(BlockHeader) + size;
            BlockHeader *newBlock = reinterpret_cast<BlockHeader *>(newBlockAddress);

            newBlock->size = selectedBlock->size - size - sizeof(BlockHeader);
            newBlock->isFree = true;
            newBlock->next = selectedBlock->next;
            newBlock->prev = selectedBlock;

            if (newBlock->next != nullptr)
            {
                newBlock->next->prev = newBlock;
            }

            selectedBlock->size = size;
            selectedBlock->next = newBlock;
        }

        selectedBlock->isFree = false;
        return static_cast<void *>(selectedBlock + 1);
    }

    void deallocate(void *ptr)
    {
        if (ptr == nullptr)
            return;

        BlockHeader *current = static_cast<BlockHeader *>(ptr) - 1;
        current->isFree = true;

        if (current->next != nullptr && current->next->isFree)
        {
            current->size += sizeof(BlockHeader) + current->next->size;
            current->next = current->next->next;
            if (current->next != nullptr)
            {
                current->next->prev = current;
            }
        }

        if (current->prev != nullptr && current->prev->isFree)
        {
            current->prev->size += sizeof(BlockHeader) + current->size;
            current->prev->next = current->next;
            if (current->next != nullptr)
            {
                current->next->prev = current->prev;
            }
        }
    }

    void *getPointerFromBlockId(int targetId)
    {
        BlockHeader *current = head;
        int currentId = 1;
        while (current != nullptr)
        {
            if (currentId == targetId)
            {
                if (current->isFree)
                    return nullptr;
                return static_cast<void *>(current + 1);
            }
            currentId++;
            current = current->next;
        }
        return nullptr;
    }

    void printActiveBlockIds()
    {
        BlockHeader *current = head;
        int id = 1;
        bool hasActive = false;
        std::cout << "Currently active Block IDs: ";

        while (current != nullptr)
        {
            if (!current->isFree)
            {
                std::cout << id << " ";
                hasActive = true;
            }
            id++;
            current = current->next;
        }

        if (!hasActive)
            std::cout << "None";
        std::cout << "\n";
    }

    void printMemoryMap(std::string unit)
    {
        BlockHeader *current = head;
        std::cout << "\n=== " << strategyName << " Map (" << unit << ") ===\n";

        int id = 1;
        size_t totalFreeBytes = 0;
        size_t totalUsedBytes = 0;

        while (current != nullptr)
        {
            std::cout << "[ID: " << id << " | ";
            if (current->isFree)
            {
                std::cout << "FREE";
                totalFreeBytes += current->size;
            }
            else
            {
                std::cout << "USED";
                totalUsedBytes += current->size;
            }

            double displaySize = convertFromBytes(current->size, unit);
            std::cout << ": " << std::fixed << std::setprecision(2) << displaySize << " " << unit << "] ";

            if (current->next != nullptr)
            {
                std::cout << "-> ";
            }

            id++;
            current = current->next;
        }

        std::cout << "\n\nBlocks: " << (id - 1)
                  << " | Used: " << std::fixed << std::setprecision(2) << convertFromBytes(totalUsedBytes, unit) << " " << unit
                  << " | Free: " << std::fixed << std::setprecision(2) << convertFromBytes(totalFreeBytes, unit) << " " << unit << "\n";
    }

    json getMemoryMapJson(const std::string &unit)
    {
        BlockHeader *current = head;
        json mapArray = json::array();
        int id = 1;

        while (current != nullptr)
        {
            json block;
            block["id"] = id;
            block["status"] = current->isFree ? "FREE" : "USED";
            block["sizeBytes"] = current->size;
            block["displaySize"] = convertFromBytes(current->size, unit);
            block["unit"] = unit;

            mapArray.push_back(block);
            id++;
            current = current->next;
        }
        return mapArray;
    }

    void printSummaryTable(std::string unit)
    {
        BlockHeader *current = head;
        int blockCount = 0;
        size_t totalFreeBytes = 0;
        size_t totalUsedBytes = 0;

        while (current != nullptr)
        {
            if (current->isFree)
            {
                totalFreeBytes += current->size;
            }
            else
            {
                totalUsedBytes += current->size;
            }
            blockCount++;
            current = current->next;
        }

        std::cout << "+-------------------------------------------------+\n";
        std::cout << "| " << std::left << std::setw(47) << (strategyName + " SUMMARY") << " |\n";
        std::cout << "+-------------------------------------------------+\n";
        std::cout << std::fixed << std::setprecision(2);

        std::cout << "| Total Pool Size:       "
                  << std::right << std::setw(17) << convertFromBytes(totalSize, unit)
                  << " " << std::left << std::setw(7) << unit
                  << std::right << "|\n";

        std::cout << "| Total Used Memory:     "
                  << std::right << std::setw(17) << convertFromBytes(totalUsedBytes, unit)
                  << " " << std::left << std::setw(7) << unit
                  << std::right << "|\n";

        std::cout << "| Total Free Memory:     "
                  << std::right << std::setw(17) << convertFromBytes(totalFreeBytes, unit)
                  << " " << std::left << std::setw(7) << unit
                  << std::right << "|\n";

        std::cout << "| Total Blocks:          "
                  << std::right << std::setw(17) << blockCount
                  << " " << std::left << std::setw(7) << " "
                  << std::right << "|\n";

        std::cout << "+-------------------------------------------------+\n\n";
    }
};

int main() {
    std::cout << "Initializing Multi-Strategy Memory Server on http://localhost:8080...\n";
    
    size_t poolSize = 10 * 1024 * 1024; // 10MB Pool
    MemoryAllocator allocFirst(poolSize, "FIRST-FIT STRATEGY");
    MemoryAllocator allocBest(poolSize, "BEST-FIT STRATEGY");
    MemoryAllocator allocWorst(poolSize, "WORST-FIT STRATEGY");

    httplib::Server svr;

    // FIX 1: The CORS Preflight Handler
    // This catches the browser's security check and gives React permission to send POST requests
    svr.Options(".*", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    // Endpoint 1: Get ALL Memory Maps
    svr.Get("/api/memory-map", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); 
        
        json response;
        response["firstFit"] = allocFirst.getMemoryMapJson("KB");
        response["bestFit"] = allocBest.getMemoryMapJson("KB");
        response["worstFit"] = allocWorst.getMemoryMapJson("KB");
        
        res.set_content(response.dump(), "application/json");
    });

    // Endpoint 2: Allocate Memory
    svr.Post("/api/allocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);
        
        double val = reqData["val"];
        std::string unit = reqData["unit"];
        size_t requestedBytes = convertToBytes(val, unit);
        
        void* ptrF = allocFirst.allocate(requestedBytes, AllocationStrategy::FIRST_FIT);
        void* ptrB = allocBest.allocate(requestedBytes, AllocationStrategy::BEST_FIT);
        void* ptrW = allocWorst.allocate(requestedBytes, AllocationStrategy::WORST_FIT);
        
        json response;
        if (ptrF && ptrB && ptrW) {
            allocatedPointers.push_back({ptrF, ptrB, ptrW});
            response["status"] = "SUCCESS";
        } else {
            if (ptrF) allocFirst.deallocate(ptrF);
            if (ptrB) allocBest.deallocate(ptrB);
            if (ptrW) allocWorst.deallocate(ptrW);
            response["status"] = "FAILED";
        }
        res.set_content(response.dump(), "application/json");
    });

    // Endpoint 3: Deallocate Memory
    svr.Post("/api/deallocate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json reqData = json::parse(req.body);
        int targetId = reqData["id"]; // The positional grid ID sent from React
        
        json response;
        response["status"] = "FAILED";

        // FIX 2: Translate the UI's positional ID into the actual physical memory pointer
        void* primaryPtr = allocFirst.getPointerFromBlockId(targetId);

        if (primaryPtr) {
            // Locate that pointer in our global tracker to safely free it across all 3 strategies
            auto it = std::find_if(allocatedPointers.begin(), allocatedPointers.end(),
                [&](const AllocationRecord& rec) { return rec.firstFitPtr == primaryPtr; });

            if (it != allocatedPointers.end()) {
                allocFirst.deallocate(it->firstFitPtr);
                allocBest.deallocate(it->bestFitPtr);
                allocWorst.deallocate(it->worstFitPtr);
                allocatedPointers.erase(it);
                response["status"] = "SUCCESS";
            }
        }
        res.set_content(response.dump(), "application/json");
    });

    // Cloud Port Configuration
    int port = 8080; // Default for local testing
    if (const char* env_p = std::getenv("PORT")) {
        port = std::stoi(env_p);
    }
    
    std::cout << "Starting cloud server on port " << port << "...\n";
    svr.listen("0.0.0.0", port);
    
    return 0;
}
}