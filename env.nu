def RET_BUILD_TOOLS_ROOT [] {
  return $"($env.PWD | path join toolchain)"
}

def RET_WindowsSDKDir [] {
  return (RET_BUILD_TOOLS_ROOT | path join 'Windows Kits' 10)
}

# let $VCToolsVersion = ls $"($BUILD_TOOLS_ROOT | path join 'VC\Tools\MSVC')" | where type == 'dir' | 
#                         sort-by modified --reverse | first | get name | split row "\\" | last

def RET_VCToolsVersion [] {
  return (do {
    cd (RET_BUILD_TOOLS_ROOT | path join "VC/Tools/MSVC")
    try { ls }
      | default -e []
      | where type == 'dir'
      | if ($in | is-empty) {
        error make -u { msg: 'VCToolsVersion cannot be determined.' }
      } else {}
      | sort-by modified -r
      | first
      | get name
      | path expand
      | path split
      | last
  })
}

def RET_VCToolsInstallDir [] {
  return (RET_BUILD_TOOLS_ROOT | path join 'VC\Tools\MSVC' (RET_VCToolsVersion))
}

def RET_WindowsSDKVersion [] {
  return (do {
      cd (RET_WindowsSDKDir | path join bin)
      try { ls } 
      | default -e []
      | where type == 'dir'
      | if ($in | is-empty) {
          error make -u {msg: "WindowsSDKVersion cannot be determined."}
      } else {}
      | sort-by modified -r
      | first
      | get name
  })
}

def RET_VSCMD_ARG_TGT_ARCH [] {
  return (open (RET_BUILD_TOOLS_ROOT | path join devcmd.ps1) | lines | get 9 | parse -r r#'\$env:(?<name>.*?)\s+=\s+(?<quote>["\'])(?<value>.*?)(\k<quote>)'# 
  | get value | first)
}

def RET_VSCMD_ARG_HOST_ARCH [] {
  return (open (RET_BUILD_TOOLS_ROOT | path join devcmd.ps1) | lines | get 10 | parse -r r#'\$env:(?<name>.*?)\s+=\s+(?<quote>["\'])(?<value>.*?)(\k<quote>)'# 
  | get value | first)
}

def RET_INCLUDE [] {
  return ($"(RET_VCToolsInstallDir | path join include);(RET_WindowsSDKDir | path join 'Include' (RET_WindowsSDKVersion) 'ucrt');(RET_WindowsSDKDir | path join 'Include' (RET_WindowsSDKVersion) 'shared');(RET_WindowsSDKDir | path join 'Include' (RET_WindowsSDKVersion) 'um');(RET_WindowsSDKDir | path join 'Include' (RET_WindowsSDKVersion) 'winrt');(RET_WindowsSDKDir | path join 'Include' (RET_WindowsSDKVersion) 'cppwinrt')")
}
def RET_LIB [] {
  return ($"(RET_VCToolsInstallDir | path join lib (RET_VSCMD_ARG_TGT_ARCH));(RET_WindowsSDKDir | path join Lib (RET_WindowsSDKVersion) ucrt (RET_VSCMD_ARG_TGT_ARCH));(RET_WindowsSDKDir | path join Lib (RET_WindowsSDKVersion) um (RET_VSCMD_ARG_TGT_ARCH))")
}

def RET_BUILD_TOOLS_BIN [] {
  return ($"(RET_VCToolsInstallDir | path join bin (['Host',(RET_VSCMD_ARG_HOST_ARCH)] | str join "") (RET_VSCMD_ARG_TGT_ARCH));(RET_WindowsSDKDir | path join bin (RET_WindowsSDKVersion) (RET_VSCMD_ARG_TGT_ARCH));(RET_WindowsSDKDir | path join bin (RET_WindowsSDKVersion) (RET_VSCMD_ARG_TGT_ARCH) ucrt)")
}

def RET_PATH [] {
  return ([...(RET_BUILD_TOOLS_BIN | split row ';'),
           ...($env.PATH | split row ';')])
}

export-env {
  load-env {
    "BUILD_TOOLS_ROOT": (RET_BUILD_TOOLS_ROOT),
    "WindowsSDKDir": (RET_WindowsSDKDir),
    "VCToolsInstallDir": (RET_VCToolsInstallDir),
    "WindowsSDKVersion": (RET_WindowsSDKVersion),
    "VSCMD_ARG_TGT_ARCH": (RET_VSCMD_ARG_TGT_ARCH),
    "VSCMD_ARG_HOST_ARCH": (RET_VSCMD_ARG_HOST_ARCH),
    "INCLUDE": (RET_INCLUDE).
    "LIB": (RET_LIB),
    "BUILD_TOOLS_BIN": (RET_BUILD_TOOLS_BIN),
    "PATH": (RET_PATH)
  }
}