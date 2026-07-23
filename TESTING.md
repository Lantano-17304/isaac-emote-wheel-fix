# Testing status

Date: 2026-07-12

## 2026-07-23 clean-Steam finding

- Clean Steam J460 EXE SHA-256:
  `3BDFC8BAE0DC7E334B76009D0AD45DFBB16EE5F00C06FFBC3A0094E34D44616B`.
- The clean executable naturally loaded the root `userenv.dll` proxy.
- Runtime analysis returned one signature match with structural validation at
  selector RVA `004D21DC`.
- Pre-release 0.1.0-pre.1 could crash after diagnosis with
  `userenv.DLL_unloaded`. The game releases its dynamically loaded userenv
  module quickly while the proxy's background loader thread could still be
  executing.
- Pre-release 0.1.0-pre.2 removes the proxy worker thread and loads the hook
  synchronously from the exported profile-directory call before the proxy can
  be released.
- Pre-release 0.1.0-pre.3 merges the diagnostic-install and hash-approval
  actions into one primary button. The verified clean J460 hash is enabled in
  one click; unknown hashes still require a successful passive run before the
  same button can enable them.

## Automated locally

- VS 2022 Win32 Release build with static `/MT` runtime.
- Proxy loaded under the installed basename `userenv.dll`.
- Both `GetUserProfileDirectoryA` and `GetUserProfileDirectoryW` resolved from the real system DLL.
- `GetUserProfileDirectoryW` returned the current Windows profile successfully.
- Loading the public Hook into a non-game PE32 process produced `signature_matches: 0`, made no patch, and reported `unsupported`.
- Manager first install: exit 0.
- Manager repeat install: exit 0.
- Unknown existing `userenv.dll`: refused with exit 12.
- Uninstall after installed Hook tampering: refused with exit 20 and retained both files.
- Uninstall after restoring the recorded hash: exit 0 and removed the two explicit installed DLLs.

## Already established on the development J460 executable

- The EmoteWheel selector signature has one match.
- Selector immediate is at RVA `004D21DC` and is initially `0`.
- Its native stick-vector call target lies inside the executable `.text` section.
- Changing only the selector to `1` makes the native wheel consume the right-stick group.
- Native J460 paths already read R3 for `ACTION_EMOTES` and A/R3 for confirmation.

The development executable SHA-256 is
`7122AC28779925B24E23E2416F231322B1470388BD25E2C08665AD8D53B3EA4F`, but it belongs to an environment whose load string was changed to `bootstp.dll`. It must not be advertised as the clean Steam hash.

## Required before stable release

- Acquire an independent Steam-clean J460 copy without verifying or modifying the working installation.
- Record its EXE SHA-256 and prove that it naturally loads a root `userenv.dll` proxy.
- Confirm the exact names/ordinals requested from `userenv.dll`.
- Run the manager, passive diagnostic and compatible activation against that copy.
- Complete Windows 10 and Windows 11 tests.
- Complete an online session with at least 20 emote sends and verify no new crash/desync artifacts.

Until every item above is complete, publish only as a pre-release.
