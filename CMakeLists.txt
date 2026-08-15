cmake_minimum_required(VERSION 3.13)
project(badusb_typing_race)

# Flipper Zero SDK provides flipper_build_application macro
find_package(FlipperZeroSDK REQUIRED)

flipper_build_application(
    NAME badusb_typing_race
    SRC src/main.c src/ui.c
    MANIFEST application.fam
    ICONS icons/icon_10x10.png icons/icon_70x40.png
)