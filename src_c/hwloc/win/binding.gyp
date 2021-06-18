{
  "targets": [
    {
      "target_name": "module",
      "sources": [
        "../src/include/base64.c",
        "../src/include/bind.c",
        "../src/include/bitmap.c",
        "../src/include/components.c",
        "../src/include/diff.c",
        "../src/include/distances.c",
        "../src/include/misc.c",
        "../src/include/pci-common.c",
        "../src/include/shmem.c",
        "../src/include/topology.c",
        "../src/include/topology-noos.c",
        "../src/include/topology-synthetic.c",
        "../src/include/topology-windows.c",
        "../src/include/topology-x86.c",
        "../src/include/topology-xml.c",
        "../src/include/topology-xml-nolibxml.c",
        "../src/include/traversal.c",
        "../src/include/memattrs.c",
        "../src/include/cpukinds.c",
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