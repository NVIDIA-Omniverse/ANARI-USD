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