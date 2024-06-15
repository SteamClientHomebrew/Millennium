cmake_minimum_required(VERSION 3.5.0)

project(Loader LANGUAGES CXX)
add_library(Loader SHARED "main.cc")

set_target_properties(Loader PROPERTIES OUTPUT_NAME "user32")
# set_target_properties(Loader PROPERTIES PREFIX "")
# set_target_properties(Loader PROPERTIES NO_EXPORT TRUE)