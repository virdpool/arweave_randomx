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
          "<!(pwd)/../../librandomx/build_<!(cat arch)/librandomx.a"
        ]
      }
    }
  ]
}