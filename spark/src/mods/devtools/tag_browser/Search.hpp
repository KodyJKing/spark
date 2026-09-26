#pragma once

#include <thread>
#include <mutex>
#include "State.hpp"
#include "Constants.hpp"

namespace Mod::DevTools {

    bool canReuse() {
        std::string searchStr = state.search;
        bool hasLastSearch = state.lastSearch[0] != 0;
        bool containsLastSearch = hasLastSearch && searchStr.find(state.lastSearch) != std::string::npos;
        bool filterNarrowed = state.lastGroupIdFilter == state.groupIdFilter || ids[state.lastGroupIdFilter].groupID == GROUP_ID_ALL;
        bool canReuse = containsLastSearch && filterNarrowed;
        return canReuse;
    }
    
    inline void tickSearch(bool focused) {        
        bool searchChanged = state.searchChanged();

        // Debounce search
        uint64_t tick = GetTickCount64();
        bool debounce = focused && (tick - state.lastSearchTick < 200);
        
        if (!debounce && searchChanged && state.searchMutex.try_lock() ) {
            state.searchMutex.unlock();
            
            std::thread searchThread = std::thread( [&] {
                std::lock_guard<std::mutex> lock(state.searchMutex);
                std::string searchStr = state.search;

                bool canceledSearch = false;

                if (canReuse()) {
                    // Results will be a subset of previous results, filter them.
                    std::vector<int> newResults;
                    for (int index : state.searchResults) {
                        auto tag = state.getTag(index);
                        if (state.filterTag(tag))
                            newResults.push_back(index);
                    }
                    state.searchResults = newResults;
                } else {
                    // Search from scratch
                    state.searchResults.clear();
                    int i = 0;
                    while (!canceledSearch) {
                        if (!state.tagExists(i)) 
                            break;
                        auto tag = state.getTag(i);
                        if (state.filterTag(tag))
                            state.searchResults.push_back(i);
                        i++;

                        // Cancel search if query has changed
                        if (searchStr != state.search)
                            canceledSearch = true;
                    }
                }
                if (!canceledSearch) {
                    strcpy_s( state.lastSearch, searchStr.c_str() );
                    state.lastGroupIdFilter = state.groupIdFilter;
                }
                state.lastSearchTick = GetTickCount64();
            } );
            searchThread.detach();

        }
    }

}
