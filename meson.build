project(
  'local-competitive-grader',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.10.0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++23',
    'warning_level=3',
    'buildtype=debugoptimized',
    'b_lto=true',
    'b_sanitize=address,undefined,leak',
  ],
)

subdir('src')
cmake = import('cmake')

opts = cmake.subproject_options()
opts.add_cmake_defines(
  {
    'REPROC++': true,
  },
)

reproc_proj = cmake.subproject('reproc', options: opts)
reproc_dep = reproc_proj.dependency('reproc++')

json = dependency('nlohmann_json', required: true)

executable('grade', src_files, dependencies: [json, reproc_dep])
