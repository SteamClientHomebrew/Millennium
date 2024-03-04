#pragma once
#ifdef _WIN32
#include <windows.h>
#include <metahost.h>
#include <mscoree.h>

LPCWSTR s_lpcwstr(const std::string& str);

/**
* @brief The clr_interop namespace manages Common Language Runtime (CLR) interoperations
* for executing .NET assemblies within the application.
*/
namespace clr_interop {
    /**
     * @brief The clr_base class manages Common Language Runtime (CLR) resources
     * for executing .NET assemblies within the application.
     */
    class clr_base {
    public:
        /**
         * @brief Returns the singleton instance of clr_base.
         * @return Reference to the singleton instance of clr_base.
         */
        static clr_base& instance() {
            static clr_base instance;
            return instance;
        }

        /**
         * @brief Initializes the CLR resources.
         * @return True if initialization succeeds, false otherwise.
         */
        bool initialize_clr();

        /**
         * @brief Releases the CLR resources.
         */
        void cleanup_clr();

        /**
         * @brief Initiates an update process using CLR.
         * @param param Additional parameter for the update process.
         * @return True if the update process starts successfully, false otherwise.
         */
        bool start_update(const std::string& param);
    private:
        /**
        * @brief Private constructor to prevent external instantiation.
        */
        clr_base();

        /**
         * @brief Private destructor to prevent external destruction.
         */
        ~clr_base();

        // Disable copy and assignment
        clr_base(const clr_base&) = delete;
        clr_base& operator=(const clr_base&) = delete;

        // CLR-related members
        ICLRMetaHost* pMetaHost;            /**< Pointer to the CLR meta-host interface. */
        ICLRRuntimeInfo* pRuntimeInfo;      /**< Pointer to the CLR runtime info interface. */
        ICLRRuntimeHost* pClrRuntimeHost;   /**< Pointer to the CLR runtime host interface. */
    };
}
#endif