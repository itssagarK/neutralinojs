json setTray(const json &input) {
    #if defined(_WIN32)
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    #endif
    json output;

    if(helpers::hasField(input, "menuItems")) {
        int menuCount = input["menuItems"].size();

        if (menuCount == 0) {
            output["success"] = true;
            return output;
        }

        if (menuCount > NEU_MAX_TRAY_MENU_ITEMS) {
            menuCount = NEU_MAX_TRAY_MENU_ITEMS;
        }

        // Free any previously allocated menu items beyond the new count
        for(int j = menuCount; j < NEU_MAX_TRAY_MENU_ITEMS; j++) {
            if(menus[j].id == nullptr && menus[j].text == nullptr) break;
            delete[] menus[j].id;
            delete[] menus[j].text;
            menus[j] = { nullptr, nullptr, 0, 0, nullptr, nullptr };
        }

        menus[menuCount - 1] = { nullptr, nullptr, 0, 0, nullptr, nullptr };

        int i = 0;
        for (const auto &menuItem: input["menuItems"]) {
            if (i >= menuCount) break;
            char *id = nullptr;
            char *text = helpers::cStrCopy(menuItem["text"].get<string>());
            int disabled = 0;
            int checked = 0;
            if(helpers::hasField(menuItem, "id")) {
                id = helpers::cStrCopy(menuItem["id"].get<string>());
            }
            if(helpers::hasField(menuItem, "isDisabled")) {
                disabled = menuItem["isDisabled"].get<bool>() ? 1 : 0;
            }
            if(helpers::hasField(menuItem, "isChecked")) {
                checked = menuItem["isChecked"].get<bool>() ? 1 : 0;
            }

            delete[] menus[i].id;
            delete[] menus[i].text;
            menus[i] = { id, text, disabled, checked, __handleTrayMenuItem, nullptr };
            i++;
        }
    }

    tray.menu = menus;

    if(helpers::hasField(input, "icon")) {
        string iconPath = input["icon"].get<string>();
        #if defined(__linux__)
        string fullIconPath;
        if(resources::isDirMode()) {
            fullIconPath = string(filesystem::absolute(settings::joinAppPath(iconPath)));
        }
        else {
            // Use alternating tempIconPath since tray_update()
            // doesn't update the icon if the path is the same as the previous one,
            // regardless whether the file contents changed or not.
            useOtherTempTrayIcon = !useOtherTempTrayIcon;
            string tempIconPath = settings::joinAppDataPath(
                    useOtherTempTrayIcon ?  "/.tmp/tray_icon_linux_01.png" : "/.tmp/tray_icon_linux_02.png"
            );

            string tempDirPath = settings::joinAppDataPath("/.tmp");;
            resources::extractFile(iconPath, tempIconPath);
            fullIconPath = filesystem::absolute(tempIconPath);
        }
        delete[] tray.icon;
        tray.icon = helpers::cStrCopy(fullIconPath);

        #elif defined(_WIN32)
        fs::FileReaderResult fileReaderResult = resources::getFile(iconPath);
        string iconDataStr = fileReaderResult.data;
        const char *iconData = iconDataStr.c_str();
        unsigned char *uiconData = reinterpret_cast<unsigned char*>(const_cast<char*>(iconData));
        IStream *pStream = SHCreateMemStream((BYTE *) uiconData, iconDataStr.length());
        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(pStream);
        bitmap->GetHICON(&tray.icon);
        pStream->Release();

        #elif defined(__APPLE__)
        fs::FileReaderResult fileReaderResult = resources::getFile(iconPath);
        string iconDataStr = fileReaderResult.data;
        const char *iconData = iconDataStr.c_str();
        tray.icon =
            ((id (*)(id, SEL))objc_msgSend)("NSImage"_cls, "alloc"_sel);

        id nsIconData = ((id (*)(id, SEL, const char*, int))objc_msgSend)("NSData"_cls,
                    "dataWithBytes:length:"_sel, iconData, iconDataStr.length());

        ((void (*)(id, SEL, id))objc_msgSend)(tray.icon, "initWithData:"_sel, nsIconData);

        if(helpers::hasField(input, "useTemplateIcon") && input["useTemplateIcon"].get<bool>()) {
            ((void (*)(id, SEL, BOOL))objc_msgSend)(tray.icon, "setTemplate:"_sel, YES);
        }
        #endif
    }

    if(!trayInitialized) {
        trayInitialized = tray_init(&tray) == 0;
    }
    else {
        tray_update(&tray);
    }
    #if defined(_WIN32)
    GdiplusShutdown(gdiplusToken);
    #endif
    if(trayInitialized) {
        output["success"] = true;
    }
    else {
        output["error"] = errors::makeErrorPayload(errors::NE_OS_TRAYIER);
    }
    return output;
}
