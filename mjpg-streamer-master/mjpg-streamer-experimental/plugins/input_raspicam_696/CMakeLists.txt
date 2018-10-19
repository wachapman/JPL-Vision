
if (EXISTS /opt/vc/include)
    set(HAS_RASPI ON)
else()
    set(HAS_RASPI OFF)
endif()


MJPG_STREAMER_PLUGIN_OPTION(input_raspicam_696 "Team 696 Raspberry Pi input camera plugin"
                            ONLYIF HAS_RASPI)

if (PLUGIN_INPUT_RASPICAM)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    include_directories(/opt/vc/include)
    include_directories(/opt/vc/include/interface/vcos)
    include_directories(/opt/vc/include/interface/vcos/pthreads)
    include_directories(/opt/vc/include/interface/vmcs_host)
    include_directories(/opt/vc/include/interface/vmcs_host/linux)
    include_directories(../input_raspicam)
    include_directories(../input_raspicam/mmal)

    link_directories(/opt/vc/lib)

    MJPG_STREAMER_PLUGIN_COMPILE(input_raspicam_696 input_raspicam_696.c overwrite_tif_tags.c detect_color_blobs.c yuv_color_space_image.c udp_comms.c tcp_comms.c yuv420.c get_ip_addr_str.c)

    target_link_libraries(input_raspicam_696 mmal_core mmal_util mmal_vc_client vcos bcm_host)

endif()
