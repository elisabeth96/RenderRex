add_executable(mesh_example mesh.cpp)
add_executable(network_example network.cpp)
add_executable(pointcloud_example pointcloud.cpp)
add_executable(test_example test.cpp)

target_link_libraries(mesh_example PRIVATE RenderRex)
target_link_libraries(network_example PRIVATE RenderRex)
target_link_libraries(pointcloud_example PRIVATE RenderRex)
target_link_libraries(test_example PRIVATE RenderRex)