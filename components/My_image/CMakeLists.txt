idf_component_register(
    SRCS
        "novel.c"
        "picture.c"
        "video.c"
        "config.c"
        "game.c"
    INCLUDE_DIRS
        "."
    REQUIRES lvgl
)
