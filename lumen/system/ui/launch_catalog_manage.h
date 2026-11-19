static int launcher_manage_indices(int *indices) {
    int n = 0;
    for (int i = 0; i < APP_LIMIT; i++)
        if (!app_package_deleted(launcher_order[i]) && app_package_installed(launcher_order[i]))
            indices[n++] = i;
    return n;
}
