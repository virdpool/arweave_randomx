{
  "targets": [
    {
      "target_name": "module",
      "sources": [
        "../src/module.cpp"
      ],
      "cflags_cc": [
        "-std=c++11"
      ],
      "link_settings": {
        "libraries": [
          "<!(cd)/../../librandomx/build_win/Release/randomx.lib"
        ]
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": ["/MD"]
        }
      }
    }
  ]
}