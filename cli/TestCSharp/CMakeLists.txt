cmake_minimum_required(VERSION 3.8)

project(TestCSharp VERSION 0.1.0 LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(${PROJECT_NAME}
	${CMAKE_CURRENT_LIST_DIR}/App.config
	${CMAKE_CURRENT_LIST_DIR}/Program.cs
	${CMAKE_CURRENT_LIST_DIR}/CereLink.cs
	${CMAKE_CURRENT_LIST_DIR}/Properties/AssemblyInfo.cs)
	
target_link_libraries(${PROJECT_NAME} ${LIB_NAME_EXT})

file(TO_NATIVE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../dist/bin" MY_CLI_DIST_DIR)
set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
		VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.6.1"
		WIN32_EXECUTABLE TRUE
)
set_property(
	TARGET ${PROJECT_NAME}
	PROPERTY
		VS_DOTNET_REFERENCES
			"System"
			"System.Core"
			"System.Xml.Linq"
			"System.Data.DataSetExtensions"
			"Microsoft.CSharp"
			"System.Data"
			"System.Net.Http"
			"System.Xml"
)
