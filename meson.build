project('simpleAtomicWorkerPool', 'cpp', default_options : ['warning_level=3', 'cpp_std=c++11'])
lockless_sleep_and_wake = subproject('lockless_sleep_and_wake')
lockless_sleep_and_wake_dep = lockless_sleep_and_wake.get_variable('lockless_sleep_and_wake_dep') 
lockless_sleep_and_wake_debug_dep = lockless_sleep_and_wake.get_variable('lockless_sleep_and_wake_debug_dep') 
simpleAtomicWorkerPool_dep = declare_dependency(include_directories: 'include', dependencies: [dependency('threads'), lockless_sleep_and_wake_dep])
simpleAtomicWorkerPool_debug_dep = declare_dependency(include_directories: 'include', dependencies: [dependency('threads'), lockless_sleep_and_wake_debug_dep])
