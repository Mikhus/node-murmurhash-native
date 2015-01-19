{
  'targets': [
    {
      'target_name': 'murmurhash',
      'sources': [
        'src/murmurhash/MurmurHash2.cpp',
        'src/murmurhash/MurmurHash3.cpp',
        'src/inputdata.cc',
        'src/nodemurmurhash.cc'
      ],
      'include_dirs': [
        "<!(node -e \"require('nan')\")",
        'src/murmurhash',
        'src'
      ],
      'defines': [
        'NODE_MURMURHASH_KEY_BUFFER_SIZE=1024'
      ],
      'conditions': [
        ['target_arch!="x64"', {
          'defines': [
            'NODE_MURMURHASH_DEFAULT_32BIT',
          ]
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1,
              'AdditionalOptions': [
                '/EHsc' # ExceptionHandling=1 is not enough
              ]
            }
          }
        }]
      ]
    }
  ]
}