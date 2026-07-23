set(MANAGER "${PACKAGE_DIR}/IsaacEmoteFixManager.exe")
set(PROXY "${PACKAGE_DIR}/payload/userenv.proxy.dll")
set(HOOK "${PACKAGE_DIR}/payload/emote_input_hook.dll")

foreach(FILE_PATH IN ITEMS "${MANAGER}" "${PROXY}" "${HOOK}")
  if(NOT EXISTS "${FILE_PATH}")
    message(FATAL_ERROR "Missing package file: ${FILE_PATH}")
  endif()
endforeach()

file(SHA256 "${MANAGER}" MANAGER_HASH)
file(SHA256 "${PROXY}" PROXY_HASH)
file(SHA256 "${HOOK}" HOOK_HASH)
file(WRITE "${PACKAGE_DIR}/SHA256SUMS.txt"
  "${MANAGER_HASH}  IsaacEmoteFixManager.exe\n"
  "${PROXY_HASH}  payload/userenv.proxy.dll\n"
  "${HOOK_HASH}  payload/emote_input_hook.dll\n")
