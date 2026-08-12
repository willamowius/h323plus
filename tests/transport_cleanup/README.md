# Transport cleanup regression test

This test covers a race in `H323Transport::CleanUpOnTermination()`. The old
implementation waited ten seconds for the attached signalling thread and then
deleted its `PThread` object regardless of whether `Main()` was still running.
An outbound H.225 thread could subsequently access its freed thread object and
`H323Connection`.

The test deliberately runs an attached thread for 12.5 seconds without a
cancellation point. Its destructor records a failure if the thread object is
deleted before the operating-system thread has stopped. On POSIX, deferred
cancellation lets the test thread return normally after about 12.5 seconds. On
Windows, PTLib forcibly terminates it after the ten-second cleanup timeout.

Start with sibling `ptlib` and `h323plus` source directories. The H323Plus
checkout must contain this test and the corresponding transport fix; when
testing a patch, apply it before building.

## Ubuntu or Debian Linux VM

Install the build dependencies in a fresh VM, then clone both repositories:

```sh
sudo apt-get update
sudo apt-get install -y build-essential git pkg-config autoconf automake \
  flex bison libssl-dev
git clone https://github.com/willamowius/ptlib.git
git clone https://github.com/willamowius/h323plus.git
# Apply the patch to h323plus here, for example:
# git -C h323plus am /path/to/0001-fix.patch
```

Configure static release builds and run the test:

```sh
cd ptlib
./configure --disable-odbc --disable-sdl --disable-lua --disable-expat
make -j"$(nproc)" optnoshared
cd ../h323plus
export PTLIBDIR="$(cd ../ptlib && pwd)"
export OPENH323DIR="$PWD"
./configure --disable-h235 --disable-h235-256
make -j"$(nproc)" optnoshared
make -C tests/transport_cleanup optnoshared
TEST_BINARY="$(find tests/transport_cleanup -type f \
  -name transport_cleanup_test -print -quit)"
test -n "$TEST_BINARY"
"$TEST_BINARY"
```

The current PTLib `master` branch matches H323Plus upstream CI. To reproduce
the XMeeting dependency exactly on Linux, check out PTLib tag `v2_10_9_6`
before configuring; the test supports both versions.

A successful run takes about 13 seconds and prints:

```text
PASS: transport waited for thread termination before deletion
```

The test was verified on Ubuntu 24.04 x86-64 against both sides of the change.
The unmodified parent revision reports H323Plus's `Transport thread did not
terminate` assertion followed by:

```text
FAIL: transport deleted a thread whose Main() was running
```

and exits with status 1. With the transport cleanup fix applied, it prints the
PASS result above and exits with status 0.

## Windows 10 or 11 VM

Install Visual Studio 2022 with **Desktop development with C++**, Git, and a
Windows 10/11 SDK. In a **Developer Command Prompt for VS 2022**, clone current
PTLib and H323Plus beside one another, then apply the patch being tested:

```bat
mkdir h323plus-cleanup-test
cd h323plus-cleanup-test
git clone https://github.com/willamowius/ptlib.git
git clone https://github.com/willamowius/h323plus.git
rem Apply the patch to h323plus here, for example:
rem git -C h323plus am C:\path\to\0001-fix.patch
cd h323plus
msbuild h323plus_2022.sln /m /p:Configuration=Release /p:Platform=x64
msbuild tests\transport_cleanup\transport_cleanup_2022.vcxproj /m /p:Configuration=Release /p:Platform=x64
tests\transport_cleanup\Release_x64\transport_cleanup_test.exe
```

A successful run prints:

```text
PASS: transport waited for thread termination before deletion
```

Expect the Windows run to take about ten seconds. Use current PTLib for this
clean-VM procedure: the older `v2_10_9_6` tag predates fixes to its Visual
Studio 2022 configure-project bootstrap. Windows PTLib uses
`TerminateThread()` for forced termination, while POSIX PTLib uses deferred
`pthread_cancel()`; the test verifies safe deletion with both behaviors.
