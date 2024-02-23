#pragma once
#include <windows.h>
#include <metahost.h>
#include <mscoree.h>

namespace clr_interop {

    class clr_base {
    public:
        static clr_base& instance() {
            static clr_base instance;
            return instance;
        }

        bool initialize_clr();
        void cleanup_clr();
        bool start_update(const std::string& param);
        bool check_millennium_updates();

        // Other methods or members can be added if needed

    private:
        clr_base();
        ~clr_base();

        // Disable copy and assignment
        clr_base(const clr_base&) = delete;
        clr_base& operator=(const clr_base&) = delete;

        // CLR-related members
        ICLRMetaHost* pMetaHost;
        ICLRRuntimeInfo* pRuntimeInfo;
        ICLRRuntimeHost* pClrRuntimeHost;

        // Helper methods for CLR initialization and cleanup
    };

    //// Function to start the update using the singleton instance
    //bool start_update(const std::string& param) {
    //    return clr_base::instance().start_update(param);
    //}

}