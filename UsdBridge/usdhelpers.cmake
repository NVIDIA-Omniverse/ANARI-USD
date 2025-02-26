macro(cleanup_include_interface)
  get_property(include_dirs
    TARGET ${cur_target}
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES)

  set(new_include_dirs "")
  foreach(incdir ${include_dirs})
    if(NOT (${incdir} MATCHES "[pP]ython"))
      list(APPEND new_include_dirs ${incdir})
    endif()
  endforeach()

  set_property(TARGET ${cur_target}
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    ${new_include_dirs})
endmacro()

macro(cleanup_link_interface)
  get_property(new_targets
    TARGET ${cur_target}
    PROPERTY INTERFACE_LINK_LIBRARIES)
  # Clear the target's transitive dependencies, otherwise those will still be taken into account by target_link_libraries
  set_property(TARGET ${cur_target}
    PROPERTY INTERFACE_LINK_LIBRARIES
    "")
endmacro()

function(flatten_and_cleanup_targets RESULT_TARGETS INIT_TARGETS)
  if(WIN32)
    set(MATCH_SUFFIX "\.lib")
  else()
    set(MATCH_SUFFIX "\.so")
  endif()
  set(candidate_targets ${INIT_TARGETS})
  set(flat_targets "")
  list(LENGTH candidate_targets num_candidates)
  while(${num_candidates} GREATER 0)
    list(POP_FRONT candidate_targets cur_target)
    list(FIND flat_targets ${cur_target} target_processed)

    if((target_processed EQUAL -1) 
      AND NOT (${cur_target} MATCHES ${MATCH_SUFFIX})
      AND NOT (${cur_target} STREQUAL "dl")
      AND NOT (${cur_target} STREQUAL "m")
      AND NOT (${cur_target} STREQUAL "opengl32")
      AND NOT (${cur_target} STREQUAL "MaterialXGenMsl")
      )
      list(APPEND flat_targets ${cur_target})
      
      cleanup_include_interface()
      cleanup_link_interface()

      list(APPEND candidate_targets ${new_targets})
    endif()

    list(LENGTH candidate_targets num_candidates)
  endwhile()

  set(${RESULT_TARGETS} ${flat_targets} PARENT_SCOPE)
endfunction()

# Define the function
function(check_file_and_substring FILE_PATH SUBSTRING OUTPUT_VAR)
    set(${OUTPUT_VAR} OFF PARENT_SCOPE)

    # Check if the file exists and contains a substring
    if(EXISTS "${FILE_PATH}")
        file(READ "${FILE_PATH}" FILE_CONTENTS)
        string(FIND "${FILE_CONTENTS}" "${SUBSTRING}" SUBSTRING_POSITION)

        if(NOT ${SUBSTRING_POSITION} EQUAL -1)
            set(${OUTPUT_VAR} ON PARENT_SCOPE)
        endif()
    endif()
endfunction()