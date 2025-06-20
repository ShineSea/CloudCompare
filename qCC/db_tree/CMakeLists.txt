
set(header_list
   ${header_list}
   ${CMAKE_CURRENT_SOURCE_DIR}/ccDBRoot.h
   ${CMAKE_CURRENT_SOURCE_DIR}/ccPropertiesTreeDelegate.h
   ${CMAKE_CURRENT_SOURCE_DIR}/matrixDisplayDlg.h
   ${CMAKE_CURRENT_SOURCE_DIR}/sfEditDlg.h
   ${CMAKE_CURRENT_SOURCE_DIR}/labelInfoEditDlg.h
   PARENT_SCOPE
)

set(source_list
   ${source_list}
   ${CMAKE_CURRENT_SOURCE_DIR}/ccDBRoot.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ccPropertiesTreeDelegate.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/matrixDisplayDlg.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/sfEditDlg.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/labelInfoEditDlg.cpp
   PARENT_SCOPE
)
