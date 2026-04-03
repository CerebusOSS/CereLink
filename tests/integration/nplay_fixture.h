///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   nplay_fixture.h
/// @brief  GTest fixture that starts/stops an nPlayServer subprocess
///
/// Downloads nPlayServer and test data from the CereLink GitHub release at
/// CMake configure time (see CMakeLists.txt), then manages the process
/// lifecycle in SetUp/TearDown.
///
/// On POSIX platforms the fixture also cleans up the named semaphore lockfile
/// that nPlayServer creates but does not remove on exit.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CERELINK_TESTS_NPLAY_FIXTURE_H
#define CERELINK_TESTS_NPLAY_FIXTURE_H

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <csignal>
#  include <fcntl.h>
#  include <semaphore.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

/// Paths injected by CMake via target_compile_definitions().
/// Defined as NPLAY_BINARY_PATH and NPLAY_NS6_PATH.
#ifndef NPLAY_BINARY_PATH
#  error "NPLAY_BINARY_PATH must be defined by the build system"
#endif
#ifndef NPLAY_NS6_PATH
#  error "NPLAY_NS6_PATH must be defined by the build system"
#endif

/// GTest fixture that manages an nPlayServer process.
///
/// Usage:
///   class MyTest : public NPlayFixture {};
///   TEST_F(MyTest, SomeTest) { ... }
///
class NPlayFixture : public ::testing::Test {
protected:
    void SetUp() override {
        m_lockName = "nplay_integ_" + std::to_string(
#ifdef _WIN32
            GetCurrentProcessId()
#else
            getpid()
#endif
        );

        std::string binary = NPLAY_BINARY_PATH;
        std::string ns6    = NPLAY_NS6_PATH;

#ifdef _WIN32
        std::string cmd = "\"" + binary + "\" --audio none --lockfile " + m_lockName + " -A \"" + ns6 + "\"";
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        BOOL ok = CreateProcessA(
            nullptr, const_cast<char*>(cmd.c_str()),
            nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr,
            &si, &m_pi);
        ASSERT_TRUE(ok) << "Failed to start nPlayServer: error " << GetLastError();
#else
        m_pid = fork();
        ASSERT_GE(m_pid, 0) << "fork() failed";
        if (m_pid == 0) {
            // Child
            execl(binary.c_str(), binary.c_str(),
                  "--audio", "none",
                  "--lockfile", m_lockName.c_str(),
                  "-A", ns6.c_str(),
                  nullptr);
            _exit(127);  // execl failed
        }
#endif
        // Wait for nPlayServer to bind its UDP ports
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ASSERT_TRUE(isRunning()) << "nPlayServer exited immediately";
    }

    void TearDown() override {
#ifdef _WIN32
        if (m_pi.hProcess) {
            TerminateProcess(m_pi.hProcess, 0);
            WaitForSingleObject(m_pi.hProcess, 5000);
            CloseHandle(m_pi.hProcess);
            CloseHandle(m_pi.hThread);
        }
#else
        if (m_pid > 0) {
            kill(m_pid, SIGTERM);
            int status = 0;
            for (int i = 0; i < 50; ++i) {
                if (waitpid(m_pid, &status, WNOHANG) != 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            // Force-kill if still alive
            if (waitpid(m_pid, &status, WNOHANG) == 0) {
                kill(m_pid, SIGKILL);
                waitpid(m_pid, &status, 0);
            }
            m_pid = -1;

            // Clean up the POSIX named semaphore
            std::string sem_name = "/" + m_lockName;
            sem_unlink(sem_name.c_str());
        }
#endif
    }

    bool isRunning() const {
#ifdef _WIN32
        DWORD code;
        return GetExitCodeProcess(m_pi.hProcess, &code) && code == STILL_ACTIVE;
#else
        return m_pid > 0 && kill(m_pid, 0) == 0;
#endif
    }

    /// Path to the CCF file in the test data directory (set by CMake).
    std::string getCcfPath() const {
#ifdef NPLAY_CCF_PATH
        return NPLAY_CCF_PATH;
#else
        return "";
#endif
    }

    /// Path to a CMP file bundled with the repo.
    std::string getCmpPath() const {
#ifdef NPLAY_CMP_PATH
        return NPLAY_CMP_PATH;
#else
        return "";
#endif
    }

private:
    std::string m_lockName;
#ifdef _WIN32
    PROCESS_INFORMATION m_pi = {};
#else
    pid_t m_pid = -1;
#endif
};

#endif  // CERELINK_TESTS_NPLAY_FIXTURE_H
