(function () {
  const state = {};
  let activeTab = "dashboard";
  let currentRoute = "guide";
  let lastSimpleRoute = "guide";
  let sessionDirty = false;
  let hotspotDirty = false;
  let previewUrl = "";
  let previewLoaded = false;
  let pendingLocaleOverride = "";
  const hostBridge = {
    ready: false,
    requestSeq: 0,
    pendingCommand: null,
    inFlightRequestId: "",
    timeoutHandle: 0,
    lastBlockedReason: "",
    lastStatusKey: "",
  };

  const debugState = {
    entries: [],
    maxEntries: 120,
    domProbeCount: 0,
  };

  const guidedState = {
    initializedFromSnapshot: false,
    choiceMode: "",
    activeMode: "",
    startedAt: 0,
    lastCommandAt: 0,
    status: "idle",
    notice: "",
    issue: null,
    actions: createGuidedActions(),
    pendingHostOpen: false,
    hostWindowHint: "",
    justStopped: false,
  };

  function makeLocalMessage(en, zhCN, zhTW) {
    return {
      en,
      "zh-CN": zhCN,
      "zh-TW": zhTW || zhCN,
      ko: en,
      ja: en,
      de: en,
      ru: en,
    };
  }

  const LOCAL_CATALOG = {
    guide_tagline: makeLocalMessage("A guided sharing flow that stays easy to use.", "专业但易用的共享引导", "專業但易用的共享引導"),
    guide_question: makeLocalMessage("Is the viewer device on the same Wi-Fi or router as this computer?", "接收方设备现在和这台电脑连的是同一个 Wi-Fi / 路由器吗？", "接收方裝置現在和這台電腦連的是同一個 Wi‑Fi / 路由器嗎？"),
    guide_intro: makeLocalMessage("Answer this one question first. The app will prepare the sharing environment automatically, and you only need to choose what to share at the end.", "先回答这一个问题，系统就会自动准备共享环境，你只需要在最后选择共享内容即可。", "先回答這一個問題，系統就會自動準備分享環境，你只需要在最後選擇分享內容即可。"),
    guide_recommended: makeLocalMessage("Recommended path", "推荐路径", "推薦路徑"),
    guide_same_network_title: makeLocalMessage("Yes, on the same network", "是，在同一个网络", "是，在同一個網路"),
    guide_same_network_detail: makeLocalMessage("Best for office networks, home routers, or devices on the same Wi-Fi. The app will choose the sharing address and start the service automatically.", "适合办公室、家庭路由器或同一个 Wi-Fi 下的设备。系统会自动选择共享地址并启动服务。", "適合辦公室、家庭路由器或同一個 Wi‑Fi 下的裝置。系統會自動選擇分享位址並啟動服務。"),
    guide_same_network_action: makeLocalMessage("Use same-network sharing", "使用同网共享", "使用同網分享"),
    guide_hotspot_title: makeLocalMessage("No, use this PC as a hotspot", "否，使用这台电脑开热点", "否，使用這台電腦開熱點"),
    guide_hotspot_detail: makeLocalMessage("Use this when there is no shared router, for quick demos, or when the on-site network is complicated. The app will prepare hotspot info first and then create the connection entry.", "适合没有统一路由器、临时演示或现场环境复杂时使用。系统会先准备热点信息，再生成连接入口。", "適合沒有統一路由器、臨時演示或現場網路複雜時使用。系統會先準備熱點資訊，再產生連線入口。"),
    guide_hotspot_action: makeLocalMessage("Use this PC hotspot", "使用本机热点", "使用本機熱點"),
    guide_auto_title: makeLocalMessage("I'm not sure", "我不确定", "我不確定"),
    guide_auto_detail: makeLocalMessage("The app tries same-network sharing first. If it cannot find a stable path, it will switch to hotspot setup and show which path is active.", "系统会先尝试同网共享，检测不到稳定路径时会切换到热点准备，并明确告诉你当前走的是哪条路。", "系統會先嘗試同網分享，偵測不到穩定路徑時會切換到熱點準備，並清楚告訴你目前走的是哪條路。"),
    guide_auto_action: makeLocalMessage("Let the app decide", "让系统判断", "讓系統判斷"),
    guide_environment_hint: makeLocalMessage("Only the key information that decides the current path is shown here.", "只展示决定当前路径的关键信息", "只展示決定目前路徑的關鍵資訊"),
    guide_resume: makeLocalMessage("Resume current sharing", "继续当前共享", "繼續目前分享"),
    auto_prepare: makeLocalMessage("Auto prepare", "自动准备", "自動準備"),
    organizing_path: makeLocalMessage("Preparing the sharing path", "系统正在整理共享路径", "系統正在整理分享路徑"),
    share_page_badge: makeLocalMessage("Share page", "共享页面", "分享頁面"),
    share_page_hint: makeLocalMessage("Hand the connection method to the viewer, then start sharing content.", "把连接方式交给对方，然后开始共享内容", "把連線方式交給對方，然後開始分享內容"),
    issue_to_resolve: makeLocalMessage("Issue to resolve", "需要处理的问题", "需要處理的問題"),
    no_blocking_issue: makeLocalMessage("There is no blocking issue right now.", "当前没有阻塞问题。", "目前沒有阻塞問題。"),
    waiting_detection: makeLocalMessage("Waiting for detection", "等待检测", "等待偵測"),
    processing: makeLocalMessage("Working", "处理中", "處理中"),
    back_to_choices: makeLocalMessage("Back to choices", "返回选择", "返回選擇"),
    continue_processing: makeLocalMessage("Continue", "继续处理", "繼續處理"),
    connection_method: makeLocalMessage("Connection method", "连接方式", "連線方式"),
    readiness: makeLocalMessage("Readiness", "准备状态", "準備狀態"),
    current_path: makeLocalMessage("Current path", "当前路径", "目前路徑"),
    sharing_address: makeLocalMessage("Sharing address", "共享地址", "分享位址"),
    hotspot_name: makeLocalMessage("Hotspot name", "热点名称", "熱點名稱"),
    hotspot_status: makeLocalMessage("Hotspot status", "热点状态", "熱點狀態"),
    hotspot_password: makeLocalMessage("Hotspot password", "热点密码", "熱點密碼"),
    hotspot_control: makeLocalMessage("Hotspot control", "热点控制", "熱點控制"),
    same_network: makeLocalMessage("Same network", "同一网络", "同一網路"),
    this_pc_hotspot: makeLocalMessage("This PC hotspot", "本机热点", "本機熱點"),
    auto_detect: makeLocalMessage("Auto detect", "自动判断", "自動判斷"),
    auto_detect_hotspot: makeLocalMessage("Auto detect, now switched to this PC hotspot", "自动判断，当前已切换为本机热点", "自動判斷，目前已切換為本機熱點"),
    auto_detect_lan: makeLocalMessage("Auto detect, currently trying same-network sharing", "自动判断，当前正在尝试同网共享", "自動判斷，目前正在嘗試同網分享"),
    auto_detect_notice: makeLocalMessage("The app tries same-network sharing first and switches to this PC hotspot only when needed.", "系统先尝试同网共享，必要时会自动切换到本机热点。", "系統先嘗試同網分享，必要時會自動切換到本機熱點。"),
    switched_to_hotspot_notice: makeLocalMessage("No stable same-network path was found. Switching to this PC hotspot.", "未检测到稳定的同网路径，正在改用本机热点。", "未偵測到穩定的同網路徑，正在改用本機熱點。"),
    direct_start: makeLocalMessage("Can start directly", "可直接启动", "可直接啟動"),
    system_settings_required: makeLocalMessage("Needs system settings", "需系统设置", "需系統設定"),
    service_started: makeLocalMessage("Service started", "服务已启动", "服務已啟動"),
    not_started_yet: makeLocalMessage("Not started yet", "尚未开始", "尚未開始"),
    local_health: makeLocalMessage("Local health", "本地健康", "本地健康"),
    access_link: makeLocalMessage("Access link", "访问地址", "存取連結"),
    local_host_page: makeLocalMessage("Local host page", "本机控制页", "本機控制頁"),
    current_status: makeLocalMessage("Current status", "当前状态", "目前狀態"),
    viewers_connected: makeLocalMessage("Viewers connected", "连接人数", "連線人數"),
    recent_handoff: makeLocalMessage("Recent handoff", "最近交付", "最近交付"),
    generated_after_prepare: makeLocalMessage("Generated after preparation", "准备后自动生成", "準備後自動產生"),
    not_used: makeLocalMessage("(not used)", "(未使用)", "(未使用)"),
    unnamed: makeLocalMessage("(unnamed)", "(未命名)", "(未命名)"),
    not_generated: makeLocalMessage("(not generated)", "(未生成)", "(未產生)"),
    waiting_for_detection: makeLocalMessage("(waiting for detection)", "(等待检测)", "(等待偵測)"),
    generating: makeLocalMessage("(generating)", "(生成中)", "(產生中)"),
    prepare_complete: makeLocalMessage("Preparation complete", "准备完成", "準備完成"),
    prepare_ready_badge: makeLocalMessage("Ready", "已准备", "已準備"),
    needs_attention: makeLocalMessage("Needs attention", "需要处理", "需要處理"),
    choose_share_content: makeLocalMessage("Choose content to share", "选择共享内容", "選擇分享內容"),
    waiting_content_selection: makeLocalMessage("Waiting for content selection", "等待选择共享内容", "等待選擇分享內容"),
    ready_to_start: makeLocalMessage("Ready to start", "待开始", "待開始"),
    share_one_more_step: makeLocalMessage("One more step before sharing", "共享前还差一步", "分享前還差一步"),
    fix_it: makeLocalMessage("Fix it", "去处理", "去處理"),
    no_extra_reminder: makeLocalMessage("There are no extra reminders right now.", "当前没有额外提醒。", "目前沒有額外提醒。"),
    connect_viewer: makeLocalMessage("Connect the viewer", "让对方连接", "讓對方連線"),
    current_connection_method: makeLocalMessage("Current connection method", "当前连接方式", "目前連線方式"),
    back_to_simple_mode: makeLocalMessage("Back to simple mode", "返回简化模式", "返回簡化模式"),
    waiting_for_you: makeLocalMessage("Waiting for you to choose what to share", "正在等待你选择共享内容", "正在等待你選擇分享內容"),
    choosing: makeLocalMessage("Choosing", "选择中", "選擇中"),
    viewer_connected: makeLocalMessage("A viewer is connected", "已有设备连接", "已有裝置連線"),
    sharing_started: makeLocalMessage("Sharing has started and the viewer is already connected. You can keep sharing or end this session.", "共享已经开始，对方设备已经连入。你可以继续共享，或者结束本次共享。", "分享已開始，對方裝置已連入。你可以繼續分享，或者結束本次分享。"),
    sharing_in_progress: makeLocalMessage("Sharing", "正在共享", "正在分享"),
    sharing_waiting_viewer: makeLocalMessage("Sharing has started and is waiting for the viewer to connect. The viewer can scan the QR code or open the link.", "共享已经开始，正在等待对方连接。你可以让对方扫码或打开地址进入。", "分享已開始，正在等待對方連線。你可以讓對方掃碼或打開位址進入。"),
    sharing_stopped: makeLocalMessage("Sharing stopped", "共享已停止", "分享已停止"),
    sharing_stopped_detail: makeLocalMessage("This sharing session has ended. You can start again or return to the guide to choose a connection method.", "本次共享已经结束。你可以重新开始，或返回引导重新选择连接方式。", "本次分享已結束。你可以重新開始，或返回引導重新選擇連線方式。"),
    sharing_not_ready: makeLocalMessage("Prepare the sharing environment first", "请先准备共享环境", "請先準備分享環境"),
    sharing_not_ready_detail: makeLocalMessage("Return to the guide. The app will prepare the network path and sharing service first, then you can choose what to share here.", "返回引导页，系统会先帮你准备网络路径和共享服务，再进入这里选择共享内容。", "返回引導頁，系統會先幫你準備網路路徑和分享服務，再進入這裡選擇分享內容。"),
    not_ready: makeLocalMessage("Not ready", "未准备", "未準備"),
    share_service_not_ready: makeLocalMessage("The sharing service is not ready yet", "共享服务尚未准备好", "分享服務尚未準備好"),
    back_to_guide: makeLocalMessage("Back to guide", "返回引导", "返回引導"),
    address_not_ready: makeLocalMessage("The current sharing address is not usable yet", "当前共享地址还不可用", "目前分享位址還不可用"),
    open_firewall_settings: makeLocalMessage("Open firewall settings", "打开防火墙设置", "打開防火牆設定"),
    verify_access_path: makeLocalMessage("Verify the access path first", "建议先验证访问路径", "建議先驗證存取路徑"),
    run_network_diagnostics: makeLocalMessage("Run network diagnostics", "运行网络诊断", "執行網路診斷"),
    qr_code: makeLocalMessage("QR code", "二维码", "QR Code"),
    click_show_qr: makeLocalMessage("Click Show QR Code to open", "点击“显示二维码”打开", "點擊「顯示 QR Code」開啟"),
    normal_status: makeLocalMessage("Normal", "正常", "正常"),
    handoff_status: makeLocalMessage("Handoff status", "交付状态", "交付狀態"),
    latest_note: makeLocalMessage("Latest note", "最近提示", "最近提示"),
    next_step_after_prepare: makeLocalMessage("The next step will be suggested after preparation completes.", "准备完成后会提示下一步", "準備完成後會提示下一步"),
    start_sharing_content: makeLocalMessage("Start sharing content", "开始共享内容", "開始分享內容"),
    let_viewer_join: makeLocalMessage("Let the viewer join", "让对方进入", "讓對方進入"),
    keep_sharing: makeLocalMessage("Keep sharing", "保持共享", "保持分享"),
    hotspot_info: makeLocalMessage("Hotspot info", "热点信息", "熱點資訊"),
    access_method: makeLocalMessage("How to connect", "访问方式", "存取方式"),
    preparing_to_share: makeLocalMessage("Preparing to share", "正在准备共享", "正在準備分享"),
    waiting_for_content: makeLocalMessage("Waiting for content", "等待选择内容", "等待選擇內容"),
    shared_target: makeLocalMessage("Shared target", "共享目标", "分享目標"),
    waiting_for_selection: makeLocalMessage("Waiting for selection", "等待选择中", "等待選擇中"),
    not_selected_yet: makeLocalMessage("Not selected yet", "尚未选择", "尚未選擇"),
    already_sharing: makeLocalMessage("Already sharing", "已经开始共享", "已經開始分享"),
    change_shared_window: makeLocalMessage("Change shared window", "更换共享窗口", "更換分享視窗"),
    change_connection_method: makeLocalMessage("Change connection method", "更换连接方式", "更換連線方式"),
    refresh_status: makeLocalMessage("Refresh status", "刷新状态", "重新整理狀態"),
    enter_share_page: makeLocalMessage("Open share page", "进入共享页面", "進入分享頁面"),
    share_service: makeLocalMessage("Sharing service", "共享服务", "分享服務"),
    share_service_can_continue: makeLocalMessage("Sharing service available", "共享服务可继续", "分享服務可繼續"),
    hotspot_ready: makeLocalMessage("Hotspot ready", "热点已就绪", "熱點已就緒"),
    network_detected: makeLocalMessage("Network detected", "已检测到网络", "已偵測到網路"),
    not_detected: makeLocalMessage("(not detected)", "(未检测到)", "(未偵測到)"),
    repair_needed: makeLocalMessage("Needs repair", "需修复", "需修復"),
    caution_needed: makeLocalMessage("Needs attention", "需注意", "需注意"),
    no_usable_network: makeLocalMessage("No usable network detected", "未检测到可用网络", "未偵測到可用網路"),
    no_usable_network_detail: makeLocalMessage("No address suitable for same-network sharing was found. Check that this PC is connected to a network, or switch to this PC hotspot.", "当前没有找到适合同网共享的地址。请确认电脑已连到网络，或者改用本机热点。", "目前沒有找到適合同網分享的位址。請確認電腦已連上網路，或者改用本機熱點。"),
    switch_to_hotspot: makeLocalMessage("Switch to PC hotspot", "改用本机热点", "改用本機熱點"),
    lan_path_unavailable: makeLocalMessage("The current same-network path is not ready yet", "当前同网路径还不可用", "目前同網路徑還不可用"),
    lan_path_unavailable_detail: makeLocalMessage("The sharing service is already running, but the current LAN address is not stable enough to hand off yet.", "系统已经启动共享服务，但当前局域网地址还不能稳定用于交付。", "系統已啟動分享服務，但目前區域網位址還不能穩定用於交付。"),
    machine_cannot_start_hotspot: makeLocalMessage("This PC cannot start hotspot directly", "当前机器无法直接启动热点", "目前這台電腦無法直接啟動熱點"),
    hotspot_needs_windows_settings: makeLocalMessage("This PC needs Windows system settings to turn on hotspot.", "这台电脑需要通过 Windows 系统设置来开启热点。", "這台電腦需要透過 Windows 系統設定來開啟熱點。"),
    open_windows_hotspot_settings: makeLocalMessage("Open Windows hotspot settings", "打开系统热点设置", "打開系統熱點設定"),
    hotspot_start_failed: makeLocalMessage("Hotspot failed to start", "热点启动失败", "熱點啟動失敗"),
    hotspot_start_failed_detail: makeLocalMessage("The app tried to start hotspot but failed. Retry, or continue through Windows hotspot settings.", "系统尝试启动热点但没有成功。你可以重试，或者改用系统热点设置继续。", "系統嘗試啟動熱點但沒有成功。你可以重試，或者改用系統熱點設定繼續。"),
    retry_hotspot: makeLocalMessage("Retry hotspot", "重试启动热点", "重試啟動熱點"),
    sharing_service_start_failed: makeLocalMessage("Sharing service failed to start", "共享服务启动失败", "分享服務啟動失敗"),
    sharing_service_start_failed_detail: makeLocalMessage("The app could not start the local sharing service.", "系统未能启动本地共享服务。", "系統未能啟動本地分享服務。"),
    restart_service: makeLocalMessage("Restart service", "重新启动服务", "重新啟動服務"),
    sharing_service_still_not_ready: makeLocalMessage("The sharing service is not ready yet", "共享服务还未准备好", "分享服務還未準備好"),
    sharing_service_still_not_ready_detail: makeLocalMessage("The service is running, but the health check has not passed yet.", "服务已经启动，但健康检查还没有通过。", "服務已啟動，但健康檢查還沒有通過。"),
    hotspot_name_password_detail: makeLocalMessage("The app is preparing the hotspot name and password.", "系统正在准备热点名称和密码。", "系統正在準備熱點名稱和密碼。"),
    viewer_join_hotspot_first: makeLocalMessage("The viewer needs to join this hotspot first.", "接收方需要先连接这个热点。", "接收方需要先連線這個熱點。"),
    local_service_needed_for_access: makeLocalMessage("Access info is generated only after the local sharing service starts.", "本地共享服务启动后才能生成访问入口。", "本地分享服務啟動後才能產生存取入口。"),
    preparing_connection_info: makeLocalMessage("The app will prepare the access link and QR code.", "系统会准备访问地址与二维码。", "系統會準備存取位址與 QR Code。"),
    open_share_page_after_prepare: makeLocalMessage("You can open the share page and choose what to share.", "可以进入共享页面，选择要共享的内容。", "可以進入分享頁面，選擇要分享的內容。"),
    checking_network_path_detail: makeLocalMessage("The app is checking which network path is usable on this PC.", "系统正在确认当前电脑的可用网络路径。", "系統正在確認目前這台電腦的可用網路路徑。"),
    choosing_address_detail: makeLocalMessage("If there are multiple addresses, the recommended one is used first.", "如有多个地址，系统会优先选推荐的那个。", "如果有多個位址，系統會優先選擇推薦的那個。"),
    prepare_complete_and_open_share: makeLocalMessage("Preparation complete, opening the share page.", "准备完成，即将进入共享页面", "準備完成，即將進入分享頁面"),
    prepare_blocked: makeLocalMessage("Preparation is currently blocked.", "当前准备过程被阻塞", "目前準備流程被阻塞"),
    preparing_hotspot_and_service: makeLocalMessage("Preparing this PC hotspot and sharing service.", "正在准备本机热点与共享服务", "正在準備本機熱點與分享服務"),
    preparing_same_lan: makeLocalMessage("Preparing the same-network sharing environment.", "正在准备同网共享环境", "正在準備同網分享環境"),
    current_environment_ready: makeLocalMessage("The current sharing environment is ready. You can start choosing what to share.", "系统已经准备好当前共享环境。你可以开始选择共享内容了。", "目前分享環境已準備好。你可以開始選擇要分享的內容。"),
    preparing_hotspot_then_service: makeLocalMessage("Please wait. The app will prepare hotspot info first, then start the local sharing service.", "请稍候，系统会先准备热点信息，再启动本地共享服务。", "請稍候，系統會先準備熱點資訊，再啟動本地分享服務。"),
    preparing_address_then_service: makeLocalMessage("Please wait. The app will choose the sharing address automatically and start the local sharing service.", "请稍候，系统会自动选择共享地址，并启动本地共享服务。", "請稍候，系統會自動選擇分享位址，並啟動本地分享服務。"),
    minimize_manual_steps: makeLocalMessage("The app keeps manual steps to a minimum and only stops when it really needs your input.", "系统会尽量减少手动操作，只在确实需要你介入时才停下来。", "系統會盡量減少手動操作，只有在真的需要你介入時才會停下來。"),
    share_service_ready_detail: makeLocalMessage("The sharing service is ready. Click Start Sharing, then choose what to share in the opened sharing window.", "共享服务已经准备好。点击开始共享，然后在打开的共享窗口中选择要共享的内容。", "分享服務已準備好。點擊開始分享，然後在打開的分享視窗中選擇要分享的內容。"),
    hand_link_then_share: makeLocalMessage("The viewer can scan the QR code or open the link first, then you can start sharing a window or screen.", "对方可以先扫码或打开地址，你再开始共享窗口或屏幕。", "對方可以先掃碼或打開位址，你再開始分享視窗或螢幕。"),
    share_picker_open_detail: makeLocalMessage("The system share picker is open. Choose the window or screen to share.", "系统共享选择器已经打开，请选择要共享的窗口或屏幕。", "系統分享選擇器已開啟，請選擇要分享的視窗或螢幕。"),
    share_picker_or_window_detail: makeLocalMessage("Click Start Sharing or Choose Window, then choose what to share in the opened sharing window.", "点击“开始共享”或“选择共享窗口”，然后在打开的共享窗口中选择要共享的内容。", "點擊「開始分享」或「選擇分享視窗」，然後在打開的分享視窗中選擇要分享的內容。"),
    share_picker_or_window_system_detail: makeLocalMessage("Click Start Sharing or Choose Window, then choose what to share in the system dialog.", "点击“开始共享”或“选择共享窗口”，然后在系统弹窗中选择要共享的内容。", "點擊「開始分享」或「選擇分享視窗」，然後在系統彈窗中選擇要分享的內容。"),
    viewer_connect_hotspot_then_open: makeLocalMessage("Let the viewer join the hotspot first, then scan the QR code or open the link. Choose what to share in the opened sharing window.", "请让接收方先连接热点，再扫码或打开地址。共享内容需要在打开的共享窗口中选择。", "請讓接收方先連線熱點，再掃碼或打開位址。分享內容需要在打開的分享視窗中選擇。"),
    hand_address_then_share: makeLocalMessage("You can hand the link to the viewer first, then start sharing a window or screen.", "你可以先把地址交给对方，再开始共享窗口或屏幕。", "你可以先把位址交給對方，再開始分享視窗或螢幕。"),
    share_service_missing_one_step: makeLocalMessage("The sharing service is running, but one preparation item is still unfinished.", "共享服务已启动，但还有一项准备尚未完成。", "分享服務已啟動，但還有一項準備尚未完成。"),
    share_service_missing_one_step_started: makeLocalMessage("The sharing service is running, but one preparation item is still unfinished.", "共享服务已经启动，但还有一项准备尚未完成。", "分享服務已啟動，但還有一項準備尚未完成。"),
    return_to_guide_then_prepare: makeLocalMessage("Return to the guide. Prepare the network path and sharing service there first, then come back here to choose what to share.", "请先返回引导页准备共享环境，再回来开始共享内容。", "請先返回引導頁準備分享環境，再回來開始分享內容。"),
    network_not_stable_for_sharing: makeLocalMessage("The current network address is not stable enough for sharing yet. Re-check the network.", "当前网络地址还不能稳定用于共享，建议重新检测网络。", "目前網路位址還不能穩定用於分享，建議重新偵測網路。"),
    firewall_may_block_devices: makeLocalMessage("Windows Firewall may block other devices from reaching the current sharing address.", "Windows 防火墙可能会阻止其它设备访问当前共享地址。", "Windows 防火牆可能會阻止其他裝置存取目前的分享位址。"),
    access_path_not_verified: makeLocalMessage("The current access path has not been fully verified yet. Run a network diagnostic first.", "当前访问路径还没有完全验证，建议先运行一次网络诊断。", "目前存取路徑還沒有完全驗證，建議先執行一次網路診斷。"),
    click_start_sharing_then_pick: makeLocalMessage("Click Start Sharing above. The app will open the share window. Then click Start Sharing there and choose a window or screen.", "点击上方的“开始共享”，系统会打开共享窗口；随后在共享窗口里点击 Start Sharing 并选择窗口或屏幕。", "點擊上方的「開始分享」，系統會打開分享視窗；接著在分享視窗裡點擊 Start Sharing 並選擇視窗或螢幕。"),
    viewer_join_current_hotspot: makeLocalMessage("Let the viewer join the current hotspot first, then scan the QR code or open the access link.", "让对方先连接当前热点，再扫码或打开访问地址。", "讓對方先連線目前熱點，再掃碼或打開存取位址。"),
    viewer_stay_same_network: makeLocalMessage("Keep the viewer on the same network, then scan the QR code or open the access link.", "让对方保持在同一个网络里，再扫码或打开访问地址。", "讓對方保持在同一個網路裡，再掃碼或打開存取位址。"),
    hotspot_already_started_detail: makeLocalMessage("The hotspot is active. Tell the viewer the hotspot name and password, then let them scan the QR code or open the link.", "当前热点已经启动。把热点名称和密码告诉对方后，再让对方扫码或打开地址。", "目前熱點已啟動。把熱點名稱和密碼告訴對方後，再讓對方掃碼或打開位址。"),
    prefer_link_or_qr: makeLocalMessage("Prefer letting the viewer open the link directly. If it is easier on site, click Show QR Code and let them scan it.", "建议优先让对方直接打开地址；如果现场更方便，也可以点“显示二维码”给对方扫码。", "建議優先讓對方直接打開位址；如果現場更方便，也可以點「顯示 QR Code」讓對方掃碼。"),
    connecting_share_page: makeLocalMessage("Connecting the share page. Please wait.", "正在连接共享页面，请稍候。", "正在連線分享頁面，請稍候。"),
    share_picker_system_detail: makeLocalMessage("The share picker is open. Choose a window or screen in the system dialog. Sharing starts automatically after that.", "共享选择器已经打开，请在系统弹窗中选择窗口或屏幕。完成后会自动开始共享。", "分享選擇器已開啟，請在系統彈窗中選擇視窗或螢幕。完成後會自動開始分享。"),
    already_started_change_window: makeLocalMessage("Sharing has started. You can keep sharing or change the shared window.", "当前已经开始共享。你可以继续共享，也可以更换共享窗口。", "目前已開始分享。你可以繼續分享，也可以更換分享視窗。"),
    viewer_join_hotspot_system_dialog: makeLocalMessage("Let the viewer join the hotspot first, then scan the QR code or open the link. Choose what to share in the system dialog.", "请让接收方先连接热点，再扫码或打开地址。共享内容需要在系统弹窗中选择。", "請讓接收方先連線熱點，再掃碼或打開位址。分享內容需要在系統彈窗中選擇。"),
    share_picker_system_choose: makeLocalMessage("Choose a window or screen in the system share picker. Sharing starts automatically once the selection is complete.", "请在系统弹出的共享选择器中选择窗口或屏幕。选择完成后会自动开始共享。", "請在系統彈出的分享選擇器中選擇視窗或螢幕。選擇完成後會自動開始分享。"),
    standalone_window_fallback: makeLocalMessage("The app is connecting the share page. If no dialog appears within a few seconds, it switches to a standalone share window automatically.", "系统正在连接共享页面。如果几秒内仍无弹窗，会自动切换到独立共享窗口。", "系統正在連線分享頁面。如果幾秒內仍沒有彈窗，會自動切換到獨立分享視窗。"),
    choose_window_system_dialog: makeLocalMessage("Click Start Sharing or Choose Window above, then choose the window or screen to share in the system dialog.", "点击上方的“开始共享”或“选择共享窗口”，然后在系统弹窗里选择要共享的窗口或屏幕。", "點擊上方的「開始分享」或「選擇分享視窗」，然後在系統彈窗裡選擇要分享的視窗或螢幕。"),
    share_window_open_change_later: makeLocalMessage("The share window is open. To change what you share, click Stop Sharing there first, then click Start Sharing and choose a new window or screen.", "共享窗口已打开。如果要更换共享内容，请先在共享窗口里点击 Stop Sharing，再点击 Start Sharing 重新选择窗口或屏幕。", "分享視窗已打開。如果要更換分享內容，請先在分享視窗裡點擊 Stop Sharing，再點擊 Start Sharing 重新選擇視窗或螢幕。"),
    share_window_will_open_first: makeLocalMessage("The app opens the share window first. Then click Start Sharing in the new window and choose a window or screen.", "系统会先打开共享窗口。接下来请在新打开的共享窗口里点击 Start Sharing，再选择窗口或屏幕。", "系統會先打開分享視窗。接著請在新打開的分享視窗裡點擊 Start Sharing，再選擇視窗或螢幕。"),
    share_window_open_click_start: makeLocalMessage("The share window is open. Click Start Sharing in the new window, then choose a window or screen.", "共享窗口已打开。请在新打开的共享窗口里点击 Start Sharing，然后选择窗口或屏幕。", "分享視窗已打開。請在新打開的分享視窗裡點擊 Start Sharing，然後選擇視窗或螢幕。"),
    switching_to_hotspot_for_connection: makeLocalMessage("Switching to this PC hotspot to prepare connection info.", "正在改用本机热点准备连接信息。", "正在改用本機熱點準備連線資訊。"),
    share_target_updated: makeLocalMessage("The shared target has been updated.", "共享目标已更新。", "分享目標已更新。"),
    share_selection_canceled: makeLocalMessage("You canceled the share selection. You can start again.", "你已取消共享选择，可以重新开始。", "你已取消分享選擇，可以重新開始。"),
    share_start_failed: makeLocalMessage("Sharing failed to start. Retry or open advanced diagnostics.", "共享启动失败，请重试或进入高级诊断。", "分享啟動失敗，請重試或進入進階診斷。"),
    bridge_unavailable: makeLocalMessage("Bridge unavailable", "桥接不可用", "橋接不可用"),
    bridge_live_running: makeLocalMessage("Bridge live / service running", "桥接在线 / 服务运行中", "橋接在線 / 服務運行中"),
    bridge_live: makeLocalMessage("Bridge live", "桥接在线", "橋接在線"),
    dashboard_ready_detail: makeLocalMessage("Service and host page look ready for the next sharing session.", "服务与 Host 页面已准备好下一次共享。", "服務與 Host 頁面已準備好下一次分享。"),
    dashboard_sharing_detail: makeLocalMessage("Sharing is active. The viewer link can be handed off right now.", "共享正在进行。现在可以把接收链接交给对方。", "分享正在進行。現在可以把接收連結交給對方。"),
    dashboard_error_detail: makeLocalMessage("The service is running, but live checks are still failing.", "服务已经启动，但实时检查仍然失败。", "服務已啟動，但即時檢查仍然失敗。"),
    dashboard_not_ready_detail: makeLocalMessage("Setup still needs to be completed before sharing.", "共享前仍需完成准备。", "分享前仍需完成準備。"),
    generated_after_prepare_paren: makeLocalMessage("(generated after preparation)", "(准备后自动生成)", "(準備後自動產生)"),
    not_used_plain: makeLocalMessage("Not used", "未使用", "未使用"),
    not_started_plain: makeLocalMessage("Not started", "未开始", "未開始"),
    not_running_plain: makeLocalMessage("Not started", "未启动", "未啟動"),
    started_plain: makeLocalMessage("Started", "已启动", "已啟動"),
    connected_plain: makeLocalMessage("Connected", "已连接", "已連線"),
    stopped_plain: makeLocalMessage("Stopped", "已停止", "已停止"),
    sharing_plain: makeLocalMessage("Sharing", "共享中", "分享中"),
    passed_check: makeLocalMessage("Passed", "通过", "通過"),
    failed_check: makeLocalMessage("Failed", "未通过", "未通過"),
    blocked_plain: makeLocalMessage("Blocked", "阻塞", "阻塞"),
    state_plain: makeLocalMessage("State", "状态", "狀態"),
    recheck_network: makeLocalMessage("Re-detect network", "重新检测网络", "重新偵測網路"),
    choose_window_button: makeLocalMessage("Choose share window", "选择共享窗口", "選擇分享視窗"),
    start_sharing_button: makeLocalMessage("Start sharing", "开始共享", "開始分享"),
    checking_network: makeLocalMessage("Checking network", "正在检测网络", "正在偵測網路"),
    starting_share_service: makeLocalMessage("Starting sharing service", "正在启动共享服务", "正在啟動分享服務"),
    starting_hotspot: makeLocalMessage("Starting hotspot", "正在启动热点", "正在啟動熱點"),
    generating_connection_info: makeLocalMessage("Generating connection info", "正在生成连接信息", "正在產生連線資訊"),
    generating_hotspot_info: makeLocalMessage("Generating hotspot info", "正在生成热点信息", "正在產生熱點資訊"),
    selecting_share_address: makeLocalMessage("Selecting sharing address", "正在选择共享地址", "正在選擇分享位址"),
    preparing_ellipsis: makeLocalMessage("Preparing...", "正在准备…", "正在準備…"),
    selecting_ellipsis: makeLocalMessage("Selecting...", "正在选择…", "正在選擇…"),
    finishing_share: makeLocalMessage("Stopping current share", "正在结束本次共享。", "正在結束本次分享。"),
    preparing_environment_auto_share: makeLocalMessage("The app is preparing the sharing environment. The share picker opens automatically once preparation is complete.", "系统正在准备共享环境，准备完成后会自动打开共享选择。", "系統正在準備分享環境，準備完成後會自動打開分享選擇器。"),
    opening_share_picker_system: makeLocalMessage("Opening the share picker. Please choose a window or screen in the system dialog.", "正在打开共享选择，请在系统弹窗中选择窗口或屏幕。", "正在打開分享選擇器，請在系統彈窗中選擇視窗或螢幕。"),
    starting_share_system: makeLocalMessage("Starting sharing. Please choose a window or screen in the system dialog.", "正在开始共享，请在系统弹窗中选择窗口或屏幕。", "正在開始分享，請在系統彈窗中選擇視窗或螢幕。"),
    share_picker_open_system_target: makeLocalMessage("The share picker is open. Please choose the content to share in the system dialog.", "共享选择器已经打开，请在系统弹窗中选择要共享的窗口或屏幕。", "分享選擇器已打開，請在系統彈窗中選擇要分享的視窗或螢幕。"),
    share_started_waiting_viewer_short: makeLocalMessage("Sharing has started. Waiting for the viewer to connect.", "共享已经开始，等待对方连接。", "分享已開始，等待對方連線。"),
    share_started_waiting_receiver_short: makeLocalMessage("Sharing has started. Waiting for the receiver to connect.", "共享已经开始，等待接收方连接。", "分享已開始，等待接收方連線。"),
    share_started_connected_short: makeLocalMessage("Sharing has started and the viewer is connected. You can keep sharing or stop now.", "共享已经开始，对方设备已经接入。你可以继续共享，或结束本次共享。", "分享已開始，對方裝置已連入。你可以繼續分享，或結束本次分享。"),
    share_stopped_short: makeLocalMessage("Sharing stopped.", "共享已停止。", "分享已停止。"),
    share_stopped_back_previous: makeLocalMessage("Sharing has stopped. You can start again or go back one step to change the connection method.", "共享已停止。你可以重新开始，或返回上一步更换连接方式。", "分享已停止。你可以重新開始，或返回上一步更換連線方式。"),
    share_stopped_back_guide: makeLocalMessage("Sharing has stopped. You can start again or return to the guide to choose a connection method.", "共享已停止。你可以重新开始，或返回引导重新选择连接方式。", "分享已停止。你可以重新開始，或返回引導重新選擇連線方式。"),
    share_page_unresponsive_closed: makeLocalMessage("The share page stopped responding and the current share was ended directly.", "共享页未响应，已直接结束本次共享。", "分享頁面沒有回應，已直接結束本次分享。"),
    receiver_connected_can_stop: makeLocalMessage("The receiver is connected. You can keep sharing or stop this session.", "接收方已经连接。你可以继续共享，也可以停止本次共享。", "接收方已連線。你可以繼續分享，也可以停止本次分享。"),
    embedded_share_page_fallback_pick: makeLocalMessage("The embedded share page is not responding. Switched to a standalone share window. Choose the window or screen to share there.", "嵌入共享页暂时没有响应，已切换为独立共享窗口。请在打开的共享窗口里选择要共享的窗口或屏幕。", "內嵌分享頁暫時沒有回應，已切換到獨立分享視窗。請在打開的分享視窗裡選擇要分享的視窗或螢幕。"),
    embedded_share_page_fallback_start: makeLocalMessage("The embedded share page is not responding. Switched to a standalone share window. Click Start Sharing there and choose the content to share.", "嵌入共享页暂时没有响应，已切换为独立共享窗口。请在打开的共享窗口里点击 Start Sharing 并选择要共享的内容。", "內嵌分享頁暫時沒有回應，已切換到獨立分享視窗。請在打開的分享視窗裡點擊 Start Sharing，然後選擇要分享的內容。"),
    return_to_guide_then_prepare_page: makeLocalMessage("Return to the guide page. The app will prepare the network path and sharing service there first, then come back here to choose what to share.", "返回引导页面，系统会先帮你准备网络路径和共享服务，再进入这里选择共享内容。", "返回引導頁面，系統會先幫你準備網路路徑和分享服務，再進來這裡選擇分享內容。"),
    viewer_connected_keep_sharing: makeLocalMessage("The viewer is already connected. You can keep sharing or end this session when finished.", "对方已经连接。你可以继续共享，或在完成后结束本次共享。", "對方已經連線。你可以繼續分享，或在完成後結束本次分享。"),
  };

  const LOCAL_OVERRIDES = {
    ko: {
      guide_tagline: "쉽지만 전문적인 공유 안내",
      guide_question: "받는 장치가 지금 이 PC와 같은 Wi-Fi 또는 라우터에 연결되어 있나요?",
      guide_intro: "이 질문만 먼저 답하면 앱이 공유 환경을 자동으로 준비합니다. 마지막에 공유할 내용만 선택하면 됩니다.",
      guide_recommended: "권장 경로",
      guide_same_network_title: "예, 같은 네트워크입니다",
      guide_same_network_detail: "사무실, 가정용 라우터, 같은 Wi-Fi에 있는 장치에 적합합니다. 앱이 공유 주소를 자동으로 고르고 서비스를 시작합니다.",
      guide_same_network_action: "같은 네트워크로 공유",
      guide_hotspot_title: "아니요, 이 PC 핫스팟 사용",
      guide_hotspot_detail: "공유 라우터가 없거나, 임시 데모이거나, 현장 네트워크가 복잡할 때 적합합니다. 앱이 먼저 핫스팟 정보를 준비한 뒤 연결 진입점을 만듭니다.",
      guide_hotspot_action: "이 PC 핫스팟 사용",
      guide_auto_title: "잘 모르겠습니다",
      guide_auto_detail: "앱은 먼저 같은 네트워크 공유를 시도합니다. 안정적인 경로를 찾지 못하면 핫스팟 준비로 전환하고 현재 경로를 알려줍니다.",
      guide_auto_action: "앱이 판단",
      guide_environment_hint: "현재 경로를 결정하는 핵심 정보만 표시합니다.",
      guide_resume: "현재 공유 계속",
      auto_prepare: "자동 준비",
      organizing_path: "공유 경로를 준비하는 중",
      share_page_hint: "연결 방법을 상대에게 전달한 뒤 공유를 시작하세요.",
      change_connection_method: "연결 방식 변경",
      issue_to_resolve: "처리할 문제",
      no_blocking_issue: "현재 막히는 문제는 없습니다.",
      connection_method: "연결 방식",
      readiness: "준비 상태",
      hotspot_name: "핫스팟 이름",
      hotspot_password: "핫스팟 비밀번호",
      share_service: "공유 서비스",
      not_used: "(사용 안 함)",
      unnamed: "(이름 없음)",
      not_generated: "(생성 안 됨)",
      not_detected: "(감지되지 않음)",
      share_service_ready_detail: "공유 서비스가 준비되었습니다. 공유 시작을 누른 뒤 열린 공유 창에서 공유할 내용을 선택하세요.",
      hand_link_then_share: "상대가 먼저 QR 코드를 스캔하거나 링크를 연 다음, 이쪽에서 창이나 화면 공유를 시작하면 됩니다.",
      share_picker_or_window_detail: "공유 시작 또는 공유 창 선택을 누른 뒤, 열린 공유 창에서 공유할 내용을 선택하세요.",
      choose_window_system_dialog: "위의 공유 시작 또는 공유 창 선택을 누른 뒤, 시스템 대화상자에서 공유할 창이나 화면을 선택하세요.",
      hotspot_already_started_detail: "핫스팟이 이미 켜져 있습니다. 이름과 비밀번호를 상대에게 알려 준 뒤 QR 코드를 스캔하거나 주소를 열게 하세요.",
      change_shared_window: "공유 창 변경",
      refresh_status: "상태 새로 고침",
      enter_share_page: "공유 페이지로 이동",
      share_service_can_continue: "공유 서비스 사용 가능",
      hotspot_ready: "핫스팟 준비 완료",
      network_detected: "네트워크 감지됨",
      repair_needed: "수정 필요",
      caution_needed: "주의 필요",
    },
    ja: {
      guide_tagline: "使いやすさを保った共有ガイド",
      guide_question: "受信側の端末は、この PC と同じ Wi-Fi またはルーターに接続されていますか？",
      guide_intro: "まずこの質問に答えるだけで、アプリが共有環境を自動で準備します。最後に共有する内容を選ぶだけです。",
      guide_recommended: "推奨ルート",
      guide_same_network_title: "はい、同じネットワークです",
      guide_same_network_detail: "オフィスや家庭用ルーター、同じ Wi-Fi 上の端末に向いています。アプリが共有アドレスを自動で選び、サービスを開始します。",
      guide_same_network_action: "同一ネットワークで共有",
      guide_hotspot_title: "いいえ、この PC のホットスポットを使う",
      guide_hotspot_detail: "共通のルーターがない場合、短時間のデモ、現場のネットワークが複雑な場合に向いています。アプリが先にホットスポット情報を準備し、その後接続入口を作成します。",
      guide_hotspot_action: "この PC のホットスポットを使う",
      guide_auto_title: "わかりません",
      guide_auto_detail: "アプリはまず同一ネットワーク共有を試します。安定した経路が見つからない場合はホットスポット準備に切り替え、現在の経路を表示します。",
      guide_auto_action: "アプリに任せる",
      guide_environment_hint: "現在の経路を決める重要な情報だけを表示します。",
      guide_resume: "現在の共有を続ける",
      auto_prepare: "自動準備",
      organizing_path: "共有経路を準備しています",
      share_page_hint: "接続方法を相手に渡してから、共有を始めてください。",
      change_connection_method: "接続方法を変更",
      issue_to_resolve: "対処が必要な問題",
      no_blocking_issue: "現在ブロックしている問題はありません。",
      connection_method: "接続方法",
      readiness: "準備状況",
      hotspot_name: "ホットスポット名",
      hotspot_password: "ホットスポットのパスワード",
      share_service: "共有サービス",
      not_used: "（未使用）",
      unnamed: "（未命名）",
      not_generated: "（未生成）",
      not_detected: "（未検出）",
      share_service_ready_detail: "共有サービスの準備ができました。共有を開始を押して、開いた共有ウィンドウで共有する内容を選択してください。",
      hand_link_then_share: "相手は先に QR コードを読み取るかリンクを開けます。その後、この PC でウィンドウや画面共有を始めてください。",
      share_picker_or_window_detail: "共有を開始 または 共有ウィンドウを選択 を押し、開いた共有ウィンドウで共有する内容を選択してください。",
      choose_window_system_dialog: "上の 共有を開始 または 共有ウィンドウを選択 を押し、システムダイアログで共有するウィンドウまたは画面を選んでください。",
      hotspot_already_started_detail: "ホットスポットはすでに起動しています。相手に名前とパスワードを伝えてから、QR コードを読み取るかアドレスを開いてもらってください。",
      change_shared_window: "共有ウィンドウを変更",
      refresh_status: "状態を更新",
      enter_share_page: "共有ページへ移動",
      share_service_can_continue: "共有サービス利用可能",
      hotspot_ready: "ホットスポット準備完了",
      network_detected: "ネットワーク検出済み",
      repair_needed: "修正が必要",
      caution_needed: "注意が必要",
    },
    de: {
      guide_tagline: "Einfache, aber professionelle Freigabeanleitung",
      guide_question: "Ist das Empfängergerät mit demselben WLAN oder Router wie dieser PC verbunden?",
      guide_intro: "Beantworten Sie zuerst nur diese eine Frage. Die App bereitet die Freigabeumgebung automatisch vor, und am Ende wählen Sie nur noch den Freigabeinhalt aus.",
      guide_recommended: "Empfohlener Pfad",
      guide_same_network_title: "Ja, im selben Netzwerk",
      guide_same_network_detail: "Geeignet für Büro, Heimrouter oder Geräte im selben WLAN. Die App wählt die Freigabeadresse automatisch und startet den Dienst.",
      guide_same_network_action: "Im selben Netzwerk freigeben",
      guide_hotspot_title: "Nein, Hotspot dieses PCs verwenden",
      guide_hotspot_detail: "Geeignet, wenn kein gemeinsamer Router vorhanden ist, für kurze Demos oder bei komplexer Netzwerksituation vor Ort. Die App bereitet zuerst die Hotspot-Daten vor und erstellt dann den Zugang.",
      guide_hotspot_action: "Hotspot dieses PCs verwenden",
      guide_auto_title: "Ich bin nicht sicher",
      guide_auto_detail: "Die App versucht zuerst die Freigabe im selben Netzwerk. Wenn kein stabiler Pfad gefunden wird, wechselt sie zur Hotspot-Vorbereitung und zeigt den aktuellen Weg an.",
      guide_auto_action: "App entscheiden lassen",
      guide_environment_hint: "Hier werden nur die wichtigsten Informationen angezeigt, die den aktuellen Pfad bestimmen.",
      guide_resume: "Aktuelle Freigabe fortsetzen",
      auto_prepare: "Automatische Vorbereitung",
      organizing_path: "Freigabepfad wird vorbereitet",
      share_page_hint: "Geben Sie dem Gegenüber zuerst die Verbindungsdaten und starten Sie dann die Freigabe.",
      change_connection_method: "Verbindungsart ändern",
      issue_to_resolve: "Problem zur Behebung",
      no_blocking_issue: "Derzeit gibt es kein blockierendes Problem.",
      connection_method: "Verbindungsart",
      readiness: "Bereitschaft",
      hotspot_name: "Hotspot-Name",
      hotspot_password: "Hotspot-Passwort",
      share_service: "Freigabedienst",
      not_used: "(nicht verwendet)",
      unnamed: "(unbenannt)",
      not_generated: "(nicht erzeugt)",
      not_detected: "(nicht erkannt)",
      share_service_ready_detail: "Der Freigabedienst ist bereit. Klicken Sie auf Freigabe starten und wählen Sie dann im geöffneten Freigabefenster den Inhalt aus.",
      hand_link_then_share: "Das Gegenüber kann zuerst den QR-Code scannen oder den Link öffnen. Danach können Sie hier Fenster oder Bildschirm freigeben.",
      share_picker_or_window_detail: "Klicken Sie auf Freigabe starten oder Fenster auswählen und wählen Sie dann im geöffneten Freigabefenster den Inhalt aus.",
      choose_window_system_dialog: "Klicken Sie oben auf Freigabe starten oder Fenster auswählen und wählen Sie dann im Systemdialog das freizugebende Fenster oder den Bildschirm aus.",
      hotspot_already_started_detail: "Der Hotspot läuft bereits. Geben Sie Name und Passwort an das Gegenüber weiter und lassen Sie es danach den QR-Code scannen oder die Adresse öffnen.",
      change_shared_window: "Freigabefenster wechseln",
      refresh_status: "Status aktualisieren",
      enter_share_page: "Freigabeseite öffnen",
      share_service_can_continue: "Freigabedienst verfügbar",
      hotspot_ready: "Hotspot bereit",
      network_detected: "Netzwerk erkannt",
      repair_needed: "Muss behoben werden",
      caution_needed: "Achtung",
    },
    ru: {
      guide_tagline: "Простой и при этом профессиональный помощник по показу",
      guide_question: "Устройство получателя сейчас подключено к тому же Wi‑Fi или роутеру, что и этот ПК?",
      guide_intro: "Сначала ответьте только на этот вопрос. Приложение само подготовит среду для показа, а в конце вам останется лишь выбрать, что показывать.",
      guide_recommended: "Рекомендуемый путь",
      guide_same_network_title: "Да, в одной сети",
      guide_same_network_detail: "Подходит для офиса, домашнего роутера или устройств в одной сети Wi‑Fi. Приложение само выберет адрес показа и запустит службу.",
      guide_same_network_action: "Показ в одной сети",
      guide_hotspot_title: "Нет, использовать точку доступа этого ПК",
      guide_hotspot_detail: "Подходит, если нет общего роутера, для короткой демонстрации или если сеть на месте слишком сложная. Сначала приложение подготовит данные точки доступа, а потом создаст вход для подключения.",
      guide_hotspot_action: "Использовать точку доступа ПК",
      guide_auto_title: "Не уверен",
      guide_auto_detail: "Сначала приложение попробует показ в одной сети. Если стабильный путь не найден, оно переключится на подготовку точки доступа и покажет текущий маршрут.",
      guide_auto_action: "Пусть решит приложение",
      guide_environment_hint: "Здесь показывается только ключевая информация, которая определяет текущий путь.",
      guide_resume: "Продолжить текущий показ",
      auto_prepare: "Автоподготовка",
      organizing_path: "Подготавливается путь показа",
      share_page_hint: "Сначала передайте способ подключения другой стороне, затем начните показ.",
      change_connection_method: "Сменить способ подключения",
      issue_to_resolve: "Проблема, требующая решения",
      no_blocking_issue: "Сейчас нет блокирующих проблем.",
      connection_method: "Способ подключения",
      readiness: "Состояние готовности",
      hotspot_name: "Имя точки доступа",
      hotspot_password: "Пароль точки доступа",
      share_service: "Служба показа",
      not_used: "(не используется)",
      unnamed: "(без имени)",
      not_generated: "(не создано)",
      not_detected: "(не обнаружено)",
      share_service_ready_detail: "Служба показа готова. Нажмите Начать показ, затем в открывшемся окне выберите, что показывать.",
      hand_link_then_share: "Другая сторона может сначала открыть ссылку или отсканировать QR-код, а затем вы сможете начать показ окна или экрана.",
      share_picker_or_window_detail: "Нажмите Начать показ или Выбрать окно, затем в открывшемся окне выберите содержимое для показа.",
      choose_window_system_dialog: "Нажмите сверху Начать показ или Выбрать окно, затем в системном диалоге выберите окно или экран для показа.",
      hotspot_already_started_detail: "Точка доступа уже запущена. Сообщите другой стороне имя и пароль, затем попросите открыть адрес или отсканировать QR-код.",
      change_shared_window: "Сменить окно показа",
      refresh_status: "Обновить состояние",
      enter_share_page: "Открыть страницу показа",
      share_service_can_continue: "Служба показа доступна",
      hotspot_ready: "Точка доступа готова",
      network_detected: "Сеть обнаружена",
      repair_needed: "Требуется исправление",
      caution_needed: "Требуется внимание",
    },
  };

  const EXTRA_LOCAL_OVERRIDES = {
    ko: {
      access_link: "접속 주소",
      current_status: "현재 상태",
      viewers_connected: "연결된 장치 수",
      recent_handoff: "최근 전달",
      current_path: "현재 경로",
      sharing_address: "공유 주소",
      hotspot_status: "핫스팟 상태",
      local_health: "로컬 상태",
      handoff_status: "전달 상태",
      latest_note: "최근 안내",
      waiting_detection: "감지 대기",
      processing: "처리 중",
      waiting_content_selection: "공유할 내용 선택 대기",
      ready_to_start: "시작 대기",
      preparing_to_share: "공유 준비 중",
      hotspot_info: "핫스팟 정보",
      access_method: "접속 방법",
      choose_window_button: "공유 창 선택",
      start_sharing_button: "공유 시작",
      already_sharing: "이미 공유 중",
      preparing_ellipsis: "준비 중...",
      selecting_ellipsis: "선택 중...",
      generated_after_prepare: "준비 후 생성",
      click_show_qr: "QR 코드 표시를 눌러 열기",
      generated_after_prepare_paren: "(준비 후 생성)",
      waiting_for_detection: "(감지 대기 중)",
      prepare_ready_badge: "준비됨",
      normal_status: "정상",
      not_used_plain: "사용 안 함",
      not_started_plain: "시작 안 됨",
      not_running_plain: "실행 안 됨",
      started_plain: "시작됨",
      connected_plain: "연결됨",
      stopped_plain: "중지됨",
      sharing_plain: "공유 중",
      passed_check: "통과",
      failed_check: "실패",
      waiting_for_you: "공유할 내용을 선택해 주세요",
      choosing: "선택 중",
      viewer_connected: "상대 장치 연결됨",
      sharing_started: "공유가 시작되었고 상대 장치가 이미 연결되었습니다. 계속 공유하거나 이번 공유를 종료할 수 있습니다.",
      sharing_in_progress: "공유 중",
      sharing_waiting_viewer: "공유가 시작되었습니다. 상대 연결을 기다리는 중입니다. 상대는 QR 코드를 스캔하거나 링크를 열 수 있습니다.",
      sharing_stopped: "공유 중지됨",
      sharing_stopped_detail: "이번 공유가 종료되었습니다. 다시 시작하거나 안내로 돌아가 연결 방식을 다시 선택할 수 있습니다.",
      sharing_not_ready: "먼저 공유 환경을 준비하세요",
      sharing_not_ready_detail: "안내 화면으로 돌아가세요. 앱이 먼저 네트워크 경로와 공유 서비스를 준비한 뒤, 여기에서 공유할 내용을 선택할 수 있습니다.",
      share_service_not_ready: "공유 서비스가 아직 준비되지 않았습니다",
      not_ready: "준비 안 됨",
      share_one_more_step: "공유 전에 한 단계가 더 필요합니다",
      no_extra_reminder: "현재 추가 안내는 없습니다.",
      qr_code: "QR 코드",
      start_sharing_content: "공유 시작",
      let_viewer_join: "상대 연결 안내",
      keep_sharing: "공유 계속",
      shared_target: "공유 대상",
      waiting_for_content: "내용 선택 대기",
      waiting_for_selection: "선택 대기",
      not_selected_yet: "아직 선택되지 않음",
      back_to_guide: "안내로 돌아가기",
      open_firewall_settings: "방화벽 설정 열기",
      verify_access_path: "접속 경로 먼저 확인",
      run_network_diagnostics: "네트워크 진단 실행",
      recheck_network: "네트워크 다시 확인",
      share_service_missing_one_step: "공유 서비스는 실행 중이지만 준비 항목 하나가 아직 끝나지 않았습니다.",
      share_service_missing_one_step_started: "공유 서비스는 실행 중이지만 준비 항목 하나가 아직 끝나지 않았습니다.",
      return_to_guide_then_prepare_page: "안내 페이지로 돌아가세요. 앱이 거기서 먼저 네트워크 경로와 공유 서비스를 준비한 뒤, 여기로 돌아와 공유할 내용을 선택할 수 있습니다.",
      network_not_stable_for_sharing: "현재 네트워크 주소는 아직 공유에 충분히 안정적이지 않습니다. 네트워크를 다시 확인하세요.",
      firewall_may_block_devices: "Windows 방화벽이 다른 장치의 현재 공유 주소 접근을 막을 수 있습니다.",
      access_path_not_verified: "현재 접속 경로가 아직 완전히 확인되지 않았습니다. 먼저 네트워크 진단을 실행하세요.",
      click_start_sharing_then_pick: "위의 공유 시작을 누르면 앱이 공유 창을 엽니다. 그곳에서 Start Sharing을 누르고 창이나 화면을 선택하세요.",
      viewer_join_current_hotspot: "상대가 먼저 현재 핫스팟에 연결한 뒤 QR 코드를 스캔하거나 접속 주소를 여세요.",
      viewer_stay_same_network: "상대를 같은 네트워크에 둔 뒤 QR 코드를 스캔하거나 접속 주소를 여세요.",
      prefer_link_or_qr: "가능하면 상대가 먼저 링크를 직접 열게 하세요. 현장에서 더 편하면 QR 코드 표시를 눌러 스캔하게 하면 됩니다.",
      connecting_share_page: "공유 페이지에 연결 중입니다. 잠시만 기다리세요.",
      share_picker_system_detail: "공유 선택기가 열려 있습니다. 시스템 대화상자에서 창이나 화면을 선택하면 자동으로 공유가 시작됩니다.",
      already_started_change_window: "공유가 이미 시작되었습니다. 계속 공유하거나 공유 창을 바꿀 수 있습니다.",
      share_window_open_change_later: "공유 창이 열려 있습니다. 공유 내용을 바꾸려면 그 창에서 먼저 Stop Sharing을 누른 뒤 Start Sharing을 눌러 새 창이나 화면을 선택하세요.",
      share_window_will_open_first: "앱이 먼저 공유 창을 엽니다. 새 창에서 Start Sharing을 누르고 창이나 화면을 선택하세요.",
      share_window_open_click_start: "공유 창이 열려 있습니다. 새 창에서 Start Sharing을 누른 뒤 창이나 화면을 선택하세요.",
      viewer_connect_hotspot_then_open: "상대가 먼저 핫스팟에 연결한 뒤 QR 코드를 스캔하거나 링크를 여세요. 공유할 내용은 열린 공유 창에서 선택합니다.",
      hand_address_then_share: "먼저 상대에게 주소를 전달한 뒤 창이나 화면 공유를 시작할 수 있습니다.",
      share_picker_or_window_system_detail: "공유 시작 또는 공유 창 선택을 누른 뒤 시스템 대화상자에서 공유할 내용을 선택하세요.",
      viewer_connected_keep_sharing: "상대가 이미 연결되어 있습니다. 공유를 계속하거나 끝난 뒤 이번 공유를 종료할 수 있습니다.",
    },
    ja: {
      access_link: "接続先アドレス",
      current_status: "現在の状態",
      viewers_connected: "接続中の端末数",
      recent_handoff: "最近の引き渡し",
      current_path: "現在の経路",
      sharing_address: "共有アドレス",
      hotspot_status: "ホットスポット状態",
      local_health: "ローカル状態",
      handoff_status: "受け渡し状態",
      latest_note: "最新メモ",
      waiting_detection: "検出待ち",
      processing: "処理中",
      waiting_content_selection: "共有内容の選択待ち",
      ready_to_start: "開始待ち",
      preparing_to_share: "共有を準備中",
      hotspot_info: "ホットスポット情報",
      access_method: "接続方法",
      choose_window_button: "共有ウィンドウを選択",
      start_sharing_button: "共有を開始",
      already_sharing: "すでに共有中",
      preparing_ellipsis: "準備中...",
      selecting_ellipsis: "選択中...",
      generated_after_prepare: "準備後に生成",
      click_show_qr: "QR コードを表示 を押して開く",
      generated_after_prepare_paren: "（準備後に生成）",
      waiting_for_detection: "（検出待ち）",
      prepare_ready_badge: "準備完了",
      normal_status: "正常",
      not_used_plain: "未使用",
      not_started_plain: "未開始",
      not_running_plain: "未起動",
      started_plain: "起動済み",
      connected_plain: "接続済み",
      stopped_plain: "停止済み",
      sharing_plain: "共有中",
      passed_check: "合格",
      failed_check: "未通過",
      waiting_for_you: "共有する内容を選択してください",
      choosing: "選択中",
      viewer_connected: "相手の端末が接続済み",
      sharing_started: "共有はすでに開始され、相手の端末も接続済みです。共有を続けるか、この共有を終了できます。",
      sharing_in_progress: "共有中",
      sharing_waiting_viewer: "共有は開始されています。相手の接続を待っています。相手は QR コードを読み取るかリンクを開けます。",
      sharing_stopped: "共有停止",
      sharing_stopped_detail: "今回の共有は終了しました。もう一度始めるか、ガイドに戻って接続方法を選び直せます。",
      sharing_not_ready: "先に共有環境を準備してください",
      sharing_not_ready_detail: "ガイドに戻ってください。アプリが先にネットワーク経路と共有サービスを準備し、その後ここで共有内容を選べます。",
      share_service_not_ready: "共有サービスはまだ準備できていません",
      not_ready: "未準備",
      share_one_more_step: "共有前にもう一段階必要です",
      no_extra_reminder: "現在追加の注意事項はありません。",
      qr_code: "QR コード",
      start_sharing_content: "共有を開始",
      let_viewer_join: "相手を接続させる",
      keep_sharing: "共有を続ける",
      shared_target: "共有対象",
      waiting_for_content: "内容選択待ち",
      waiting_for_selection: "選択待ち",
      not_selected_yet: "まだ未選択",
      back_to_guide: "ガイドに戻る",
      open_firewall_settings: "ファイアウォール設定を開く",
      verify_access_path: "接続経路を先に確認",
      run_network_diagnostics: "ネットワーク診断を実行",
      recheck_network: "ネットワークを再確認",
      share_service_missing_one_step: "共有サービスは動作中ですが、準備項目がまだ 1 つ完了していません。",
      share_service_missing_one_step_started: "共有サービスは動作中ですが、準備項目がまだ 1 つ完了していません。",
      return_to_guide_then_prepare_page: "ガイドページに戻ってください。アプリがそこで先にネットワーク経路と共有サービスを準備し、その後ここで共有内容を選べます。",
      network_not_stable_for_sharing: "現在のネットワークアドレスは、共有に使うにはまだ十分安定していません。ネットワークを再確認してください。",
      firewall_may_block_devices: "Windows ファイアウォールが他の端末から現在の共有アドレスへアクセスするのを妨げる可能性があります。",
      access_path_not_verified: "現在の接続経路はまだ完全に確認されていません。先にネットワーク診断を実行してください。",
      click_start_sharing_then_pick: "上の共有を開始を押すとアプリが共有ウィンドウを開きます。そこで Start Sharing を押して、ウィンドウまたは画面を選択してください。",
      viewer_join_current_hotspot: "相手に先に現在のホットスポットへ接続してもらい、その後 QR コードを読み取るか接続先アドレスを開いてもらってください。",
      viewer_stay_same_network: "相手を同じネットワークに接続したままにし、その後 QR コードを読み取るか接続先アドレスを開いてもらってください。",
      prefer_link_or_qr: "できれば相手に先にリンクを直接開いてもらってください。現場でその方が楽なら QR コードを表示して読み取ってもらっても構いません。",
      connecting_share_page: "共有ページに接続しています。しばらくお待ちください。",
      share_picker_system_detail: "共有ピッカーは開いています。システムダイアログでウィンドウまたは画面を選ぶと、自動で共有が始まります。",
      already_started_change_window: "共有はすでに開始されています。共有を続けるか、共有ウィンドウを変更できます。",
      share_window_open_change_later: "共有ウィンドウは開いています。共有内容を変えるには、そのウィンドウで先に Stop Sharing を押し、その後 Start Sharing を押して新しいウィンドウまたは画面を選んでください。",
      share_window_will_open_first: "アプリが先に共有ウィンドウを開きます。新しいウィンドウで Start Sharing を押し、ウィンドウまたは画面を選んでください。",
      share_window_open_click_start: "共有ウィンドウは開いています。新しいウィンドウで Start Sharing を押してから、ウィンドウまたは画面を選んでください。",
      viewer_connect_hotspot_then_open: "相手に先にホットスポットへ接続してもらい、その後 QR コードを読み取るかリンクを開いてもらってください。共有内容は開いた共有ウィンドウで選びます。",
      hand_address_then_share: "先に相手へアドレスを渡してから、ウィンドウまたは画面の共有を始められます。",
      share_picker_or_window_system_detail: "共有を開始 または 共有ウィンドウを選択 を押し、その後システムダイアログで共有する内容を選択してください。",
      viewer_connected_keep_sharing: "相手はすでに接続されています。共有を続けるか、完了後に今回の共有を終了できます。",
    },
    de: {
      access_link: "Zugriffsadresse",
      current_status: "Aktueller Status",
      viewers_connected: "Verbundene Geräte",
      recent_handoff: "Letzte Übergabe",
      current_path: "Aktueller Pfad",
      sharing_address: "Freigabeadresse",
      hotspot_status: "Hotspot-Status",
      local_health: "Lokaler Zustand",
      handoff_status: "Übergabestatus",
      latest_note: "Letzter Hinweis",
      waiting_detection: "Warten auf Erkennung",
      processing: "Wird verarbeitet",
      waiting_content_selection: "Warten auf Freigabeinhalt",
      ready_to_start: "Startbereit",
      preparing_to_share: "Freigabe wird vorbereitet",
      hotspot_info: "Hotspot-Info",
      access_method: "Zugriffsmethode",
      choose_window_button: "Freigabefenster wählen",
      start_sharing_button: "Freigabe starten",
      already_sharing: "Freigabe läuft bereits",
      preparing_ellipsis: "Wird vorbereitet...",
      selecting_ellipsis: "Wird ausgewählt...",
      generated_after_prepare: "Wird nach der Vorbereitung erzeugt",
      click_show_qr: "Zum Öffnen auf QR-Code anzeigen klicken",
      generated_after_prepare_paren: "(wird nach der Vorbereitung erzeugt)",
      waiting_for_detection: "(Warten auf Erkennung)",
      prepare_ready_badge: "Bereit",
      normal_status: "Normal",
      not_used_plain: "Nicht verwendet",
      not_started_plain: "Nicht gestartet",
      not_running_plain: "Nicht gestartet",
      started_plain: "Gestartet",
      connected_plain: "Verbunden",
      stopped_plain: "Beendet",
      sharing_plain: "Freigabe läuft",
      passed_check: "Bestanden",
      failed_check: "Fehlgeschlagen",
      waiting_for_you: "Bitte wählen Sie den Freigabeinhalt aus",
      choosing: "Wird ausgewählt",
      viewer_connected: "Gegengerät verbunden",
      sharing_started: "Die Freigabe läuft bereits und das Gegengerät ist schon verbunden. Sie können weiter freigeben oder diese Sitzung beenden.",
      sharing_in_progress: "Freigabe läuft",
      sharing_waiting_viewer: "Die Freigabe wurde gestartet. Es wird auf die Verbindung der Gegenseite gewartet. Sie kann den QR-Code scannen oder den Link öffnen.",
      sharing_stopped: "Freigabe beendet",
      sharing_stopped_detail: "Diese Freigabe wurde beendet. Sie können erneut starten oder zum Assistenten zurückkehren und die Verbindungsart neu wählen.",
      sharing_not_ready: "Bereiten Sie zuerst die Freigabeumgebung vor",
      sharing_not_ready_detail: "Gehen Sie zum Assistenten zurück. Die App bereitet dort zuerst Netzwerkpfad und Freigabedienst vor, danach können Sie hier den Freigabeinhalt auswählen.",
      share_service_not_ready: "Der Freigabedienst ist noch nicht bereit",
      not_ready: "Nicht bereit",
      share_one_more_step: "Vor der Freigabe fehlt noch ein Schritt",
      no_extra_reminder: "Aktuell gibt es keine zusätzlichen Hinweise.",
      qr_code: "QR-Code",
      start_sharing_content: "Freigabe starten",
      let_viewer_join: "Gegenseite verbinden lassen",
      keep_sharing: "Freigabe fortsetzen",
      shared_target: "Freigegebenes Ziel",
      waiting_for_content: "Warten auf Inhalt",
      waiting_for_selection: "Warten auf Auswahl",
      not_selected_yet: "Noch nicht ausgewählt",
      back_to_guide: "Zurück zum Assistenten",
      open_firewall_settings: "Firewall-Einstellungen öffnen",
      verify_access_path: "Zugriffspfad zuerst prüfen",
      run_network_diagnostics: "Netzwerkdiagnose ausführen",
      recheck_network: "Netzwerk erneut prüfen",
      share_service_missing_one_step: "Der Freigabedienst läuft, aber ein Vorbereitungsschritt ist noch nicht abgeschlossen.",
      share_service_missing_one_step_started: "Der Freigabedienst läuft, aber ein Vorbereitungsschritt ist noch nicht abgeschlossen.",
      return_to_guide_then_prepare_page: "Gehen Sie zur Assistentenseite zurück. Dort bereitet die App zuerst Netzwerkpfad und Freigabedienst vor. Danach können Sie hier den Freigabeinhalt auswählen.",
      network_not_stable_for_sharing: "Die aktuelle Netzwerkadresse ist für die Freigabe noch nicht stabil genug. Prüfen Sie das Netzwerk erneut.",
      firewall_may_block_devices: "Die Windows-Firewall könnte andere Geräte daran hindern, die aktuelle Freigabeadresse zu erreichen.",
      access_path_not_verified: "Der aktuelle Zugriffspfad ist noch nicht vollständig geprüft. Führen Sie zuerst eine Netzwerkdiagnose aus.",
      click_start_sharing_then_pick: "Klicken Sie oben auf Freigabe starten. Die App öffnet das Freigabefenster. Klicken Sie dort auf Start Sharing und wählen Sie dann ein Fenster oder einen Bildschirm aus.",
      viewer_join_current_hotspot: "Die Gegenseite soll zuerst den aktuellen Hotspot verbinden und danach den QR-Code scannen oder die Zugriffsadresse öffnen.",
      viewer_stay_same_network: "Die Gegenseite soll im selben Netzwerk bleiben und danach den QR-Code scannen oder die Zugriffsadresse öffnen.",
      prefer_link_or_qr: "Am besten öffnet die Gegenseite zuerst direkt den Link. Falls es vor Ort einfacher ist, zeigen Sie den QR-Code an und lassen Sie ihn scannen.",
      connecting_share_page: "Verbindung zur Freigabeseite wird hergestellt. Bitte warten.",
      share_picker_system_detail: "Der Freigabeauswahldialog ist geöffnet. Wählen Sie im Systemdialog ein Fenster oder einen Bildschirm aus. Danach startet die Freigabe automatisch.",
      already_started_change_window: "Die Freigabe wurde bereits gestartet. Sie können weiter freigeben oder das freigegebene Fenster ändern.",
      share_window_open_change_later: "Das Freigabefenster ist geöffnet. Um den Freigabeinhalt zu ändern, klicken Sie dort zuerst auf Stop Sharing, dann auf Start Sharing und wählen ein neues Fenster oder einen Bildschirm.",
      share_window_will_open_first: "Die App öffnet zuerst das Freigabefenster. Klicken Sie im neuen Fenster auf Start Sharing und wählen Sie dann ein Fenster oder einen Bildschirm.",
      share_window_open_click_start: "Das Freigabefenster ist geöffnet. Klicken Sie im neuen Fenster auf Start Sharing und wählen Sie dann ein Fenster oder einen Bildschirm.",
      viewer_connect_hotspot_then_open: "Die Gegenseite soll sich zuerst mit dem Hotspot verbinden und dann den QR-Code scannen oder den Link öffnen. Der Freigabeinhalt wird im geöffneten Freigabefenster ausgewählt.",
      hand_address_then_share: "Sie können zuerst die Adresse weitergeben und danach die Freigabe eines Fensters oder Bildschirms starten.",
      share_picker_or_window_system_detail: "Klicken Sie auf Freigabe starten oder Fenster auswählen und wählen Sie dann im Systemdialog aus, was freigegeben werden soll.",
      viewer_connected_keep_sharing: "Die Gegenseite ist bereits verbunden. Sie können weiter freigeben oder diese Sitzung nach Abschluss beenden.",
    },
    ru: {
      access_link: "Адрес доступа",
      current_status: "Текущее состояние",
      viewers_connected: "Подключённые устройства",
      recent_handoff: "Последняя передача",
      current_path: "Текущий маршрут",
      sharing_address: "Адрес показа",
      hotspot_status: "Состояние точки доступа",
      local_health: "Локальное состояние",
      handoff_status: "Состояние передачи",
      latest_note: "Последняя подсказка",
      waiting_detection: "Ожидание обнаружения",
      processing: "Обработка",
      waiting_content_selection: "Ожидание выбора содержимого",
      ready_to_start: "Готово к запуску",
      preparing_to_share: "Подготовка показа",
      hotspot_info: "Данные точки доступа",
      access_method: "Способ доступа",
      choose_window_button: "Выбрать окно показа",
      start_sharing_button: "Начать показ",
      already_sharing: "Показ уже идёт",
      preparing_ellipsis: "Подготовка...",
      selecting_ellipsis: "Выбор...",
      generated_after_prepare: "Будет создано после подготовки",
      click_show_qr: "Нажмите Показать QR-код, чтобы открыть",
      generated_after_prepare_paren: "(будет создано после подготовки)",
      waiting_for_detection: "(ожидание обнаружения)",
      prepare_ready_badge: "Готово",
      normal_status: "Нормально",
      not_used_plain: "Не используется",
      not_started_plain: "Не начато",
      not_running_plain: "Не запущено",
      started_plain: "Запущено",
      connected_plain: "Подключено",
      stopped_plain: "Остановлено",
      sharing_plain: "Идёт показ",
      passed_check: "Пройдено",
      failed_check: "Не пройдено",
      waiting_for_you: "Выберите, что показывать",
      choosing: "Идёт выбор",
      viewer_connected: "Устройство другой стороны подключено",
      sharing_started: "Показ уже начался, и устройство другой стороны уже подключено. Можно продолжить показ или завершить этот сеанс.",
      sharing_in_progress: "Идёт показ",
      sharing_waiting_viewer: "Показ уже начался. Ожидается подключение другой стороны. Она может открыть ссылку или отсканировать QR-код.",
      sharing_stopped: "Показ остановлен",
      sharing_stopped_detail: "Этот сеанс показа завершён. Можно начать снова или вернуться к мастеру и заново выбрать способ подключения.",
      sharing_not_ready: "Сначала подготовьте среду для показа",
      sharing_not_ready_detail: "Вернитесь к мастеру. Там приложение сначала подготовит сетевой путь и службу показа, а затем здесь можно будет выбрать, что показывать.",
      share_service_not_ready: "Служба показа ещё не готова",
      not_ready: "Не готово",
      share_one_more_step: "Перед показом нужен ещё один шаг",
      no_extra_reminder: "Сейчас нет дополнительных подсказок.",
      qr_code: "QR-код",
      start_sharing_content: "Начать показ",
      let_viewer_join: "Подключить другую сторону",
      keep_sharing: "Продолжить показ",
      shared_target: "Показываемый объект",
      waiting_for_content: "Ожидание выбора содержимого",
      waiting_for_selection: "Ожидание выбора",
      not_selected_yet: "Пока не выбрано",
      back_to_guide: "Вернуться к мастеру",
      open_firewall_settings: "Открыть настройки брандмауэра",
      verify_access_path: "Сначала проверить путь доступа",
      run_network_diagnostics: "Запустить сетевую диагностику",
      recheck_network: "Проверить сеть ещё раз",
      share_service_missing_one_step: "Служба показа уже работает, но один шаг подготовки ещё не завершён.",
      share_service_missing_one_step_started: "Служба показа уже работает, но один шаг подготовки ещё не завершён.",
      return_to_guide_then_prepare_page: "Вернитесь на страницу мастера. Там приложение сначала подготовит сетевой путь и службу показа, а затем вы сможете выбрать здесь, что показывать.",
      network_not_stable_for_sharing: "Текущий сетевой адрес пока недостаточно стабилен для показа. Проверьте сеть ещё раз.",
      firewall_may_block_devices: "Брандмауэр Windows может мешать другим устройствам открыть текущий адрес показа.",
      access_path_not_verified: "Текущий путь доступа ещё не проверен полностью. Сначала запустите сетевую диагностику.",
      click_start_sharing_then_pick: "Нажмите сверху Начать показ. Приложение откроет окно показа. Затем нажмите там Start Sharing и выберите окно или экран.",
      viewer_join_current_hotspot: "Сначала попросите другую сторону подключиться к текущей точке доступа, затем открыть адрес доступа или отсканировать QR-код.",
      viewer_stay_same_network: "Пусть другая сторона остаётся в той же сети, затем откроет адрес доступа или отсканирует QR-код.",
      prefer_link_or_qr: "Лучше сначала дать другой стороне открыть ссылку напрямую. Если на месте удобнее, можно показать QR-код и попросить его отсканировать.",
      connecting_share_page: "Идёт подключение к странице показа. Подождите.",
      share_picker_system_detail: "Окно выбора показа уже открыто. Выберите в системном диалоге окно или экран, после чего показ начнётся автоматически.",
      already_started_change_window: "Показ уже начался. Можно продолжить показ или сменить показываемое окно.",
      share_window_open_change_later: "Окно показа уже открыто. Чтобы сменить содержимое, сначала нажмите там Stop Sharing, затем Start Sharing и выберите новое окно или экран.",
      share_window_will_open_first: "Сначала приложение откроет окно показа. В новом окне нажмите Start Sharing и выберите окно или экран.",
      share_window_open_click_start: "Окно показа уже открыто. В новом окне нажмите Start Sharing, затем выберите окно или экран.",
      viewer_connect_hotspot_then_open: "Сначала попросите другую сторону подключиться к точке доступа, затем открыть ссылку или отсканировать QR-код. Что именно показывать, выбирается в открытом окне показа.",
      hand_address_then_share: "Сначала можно передать адрес другой стороне, а затем начать показ окна или экрана.",
      share_picker_or_window_system_detail: "Нажмите Начать показ или Выбрать окно, а затем выберите в системном диалоге, что именно показывать.",
      viewer_connected_keep_sharing: "Другая сторона уже подключена. Можно продолжить показ или завершить этот сеанс после окончания.",
    },
  };

  const LOCAL_NORMALIZED_KEYS = new Map([
    ["stopped", "stopped_plain"],
    ["started", "started_plain"],
    ["connected", "connected_plain"],
    ["sharing", "sharing_plain"],
    ["not started", "not_started_plain"],
    ["not running", "not_running_plain"],
    ["not used", "not_used_plain"],
    ["unnamed", "unnamed"],
    ["not generated", "not_generated"],
    ["waiting for detection", "waiting_for_detection"],
    ["not detected", "not_detected"],
    ["generating", "generating"],
    ["passed", "passed_check"],
    ["failed", "failed_check"],
    ["blocked", "blocked_plain"],
    ["ready", "prepare_ready_badge"],
  ]);

  const LOCAL_REVERSE = new Map();

  function registerLocalReverseCatalog(catalog) {
    Object.keys(catalog || {}).forEach((key) => {
      const entry = catalog[key];
      Object.keys(entry || {}).forEach((locale) => {
        const value = entry[locale];
        if (typeof value === "string" && value) {
          LOCAL_REVERSE.set(value, key);
        }
      });
    });
  }

  function registerLocalReverseOverrides(overrides) {
    Object.keys(overrides || {}).forEach((locale) => {
      const entry = overrides[locale];
      Object.keys(entry || {}).forEach((key) => {
        const value = entry[key];
        if (typeof value === "string" && value) {
          LOCAL_REVERSE.set(value, key);
        }
      });
    });
  }

  registerLocalReverseCatalog(LOCAL_CATALOG);
  registerLocalReverseOverrides(LOCAL_OVERRIDES);
  registerLocalReverseOverrides(EXTRA_LOCAL_OVERRIDES);

  function currentUiLocale() {
    return window.LanShareI18n && typeof window.LanShareI18n.getLocale === "function"
      ? window.LanShareI18n.getLocale()
      : "en";
  }

  function localMessage(key) {
    const entry = LOCAL_CATALOG[key];
    if (!entry) return key;
    const locale = currentUiLocale();
    const override = (EXTRA_LOCAL_OVERRIDES[locale] && EXTRA_LOCAL_OVERRIDES[locale][key])
      || (LOCAL_OVERRIDES[locale] && LOCAL_OVERRIDES[locale][key]);
    if (override) return override;
    if (entry[locale]) return entry[locale];
    if (window.LanShareI18n && typeof window.LanShareI18n.translateText === "function") {
      const translatedEn = window.LanShareI18n.translateText(entry.en, locale);
      if (translatedEn && translatedEn !== entry.en) return translatedEn;
      const translatedZh = window.LanShareI18n.translateText(entry["zh-CN"], locale);
      if (translatedZh && translatedZh !== entry["zh-CN"]) return translatedZh;
    }
    return entry.en || "";
  }

  function translateLocalPattern(text) {
    const locale = currentUiLocale();
    let match = text.match(/^正在共享“(.+)”，对方设备已经接入。你可以继续共享，或结束本次共享。$/);
    if (match) {
      if (locale === "zh-CN") return text;
      if (locale === "zh-TW") return `正在分享「${match[1]}」，對方裝置已連入。你可以繼續分享，或結束本次分享。`;
      if (locale === "ko") return `지금 "${match[1]}"을(를) 공유 중이며 상대 장치가 이미 연결되었습니다. 계속 공유하거나 이번 공유를 종료할 수 있습니다.`;
      if (locale === "ja") return `現在「${match[1]}」を共有中で、相手の端末はすでに接続されています。共有を続けるか、この共有を終了できます。`;
      if (locale === "de") return `„${match[1]}“ wird gerade freigegeben und das Gegengerät ist bereits verbunden. Sie können die Freigabe fortsetzen oder diese Sitzung beenden.`;
      if (locale === "ru") return `Сейчас показывается «${match[1]}», устройство другой стороны уже подключено. Можно продолжить показ или завершить этот сеанс.`;
      return `Sharing "${match[1]}". The viewer is already connected. You can keep sharing or end this session.`;
    }
    match = text.match(/^正在共享“(.+)”，等待对方连接。你可以让对方扫码或打开地址进入。$/);
    if (match) {
      if (locale === "zh-CN") return text;
      if (locale === "zh-TW") return `正在分享「${match[1]}」，等待對方連線。你可以讓對方掃碼或打開位址進入。`;
      if (locale === "ko") return `지금 "${match[1]}"을(를) 공유 중이며 상대 연결을 기다리고 있습니다. 상대는 QR 코드를 스캔하거나 링크를 열 수 있습니다.`;
      if (locale === "ja") return `現在「${match[1]}」を共有中で、相手の接続を待っています。相手は QR コードを読み取るかリンクを開けます。`;
      if (locale === "de") return `„${match[1]}“ wird gerade freigegeben. Es wird auf die Verbindung der Gegenseite gewartet. Sie kann den QR-Code scannen oder den Link öffnen.`;
      if (locale === "ru") return `Сейчас показывается «${match[1]}», идёт ожидание подключения другой стороны. Она может открыть ссылку или отсканировать QR-код.`;
      return `Sharing "${match[1]}". Waiting for the viewer to connect. They can scan the QR code or open the link.`;
    }
    match = text.match(/^当前已选择“(.+)”。你可以直接开始共享，或重新选择共享窗口。$/);
    if (match) {
      if (locale === "zh-CN") return text;
      if (locale === "zh-TW") return `目前已選擇「${match[1]}」。你可以直接開始分享，或重新選擇分享視窗。`;
      if (locale === "ko") return `"${match[1]}"이(가) 이미 선택되었습니다. 지금 바로 공유를 시작하거나 다른 창을 다시 선택할 수 있습니다.`;
      if (locale === "ja") return `「${match[1]}」はすでに選択されています。今すぐ共有を開始するか、共有ウィンドウを選び直せます。`;
      if (locale === "de") return `„${match[1]}“ ist bereits ausgewählt. Sie können die Freigabe jetzt starten oder ein anderes Fenster auswählen.`;
      if (locale === "ru") return `«${match[1]}» уже выбрано. Можно сразу начать показ или выбрать другое окно.`;
      return `"${match[1]}" is already selected. You can start sharing now or choose another window.`;
    }
    match = text.match(/^当前正在共享“(.+)”。你可以继续共享，也可以更换共享窗口。$/);
    if (match) {
      if (locale === "zh-CN") return text;
      if (locale === "zh-TW") return `目前正在分享「${match[1]}」。你可以繼續分享，也可以更換分享視窗。`;
      if (locale === "ko") return `지금 "${match[1]}"을(를) 공유 중입니다. 계속 공유하거나 공유 창을 변경할 수 있습니다.`;
      if (locale === "ja") return `現在「${match[1]}」を共有中です。共有を続けるか、共有ウィンドウを変更できます。`;
      if (locale === "de") return `„${match[1]}“ wird gerade freigegeben. Sie können die Freigabe fortsetzen oder das freigegebene Fenster ändern.`;
      if (locale === "ru") return `Сейчас показывается «${match[1]}». Можно продолжить показ или сменить окно показа.`;
      return `Currently sharing "${match[1]}". You can keep sharing or change the shared window.`;
    }
    return text;
  }

  function translateLocalLiteral(text) {
    const value = String(text || "");
    if (!value.trim()) return value;
    const leading = value.match(/^\s*/)[0];
    const trailing = value.match(/\s*$/)[0];
    const core = value.trim();
    const key = LOCAL_REVERSE.get(core);
    if (key) {
      return leading + localMessage(key) + trailing;
    }
    const normalized = core
      .replace(/^[（(]\s*|\s*[）)]$/g, "")
      .replace(/[_-]+/g, " ")
      .replace(/\s+/g, " ")
      .trim()
      .toLowerCase();
    const normalizedKey = LOCAL_NORMALIZED_KEYS.get(normalized);
    if (normalizedKey) {
      return leading + localMessage(normalizedKey) + trailing;
    }
    const patterned = translateLocalPattern(core);
    return leading + patterned + trailing;
  }

  function shouldSkipLocalTranslation(el) {
    return !!(el && (el.closest("script,style,pre,code,samp,kbd,textarea,[data-no-i18n]") || el.tagName === "OPTION"));
  }

  function applyLocalTranslations(root) {
    const target = root && root.nodeType ? root : (document.body || document.documentElement);
    if (!target) return;

    const translateTextNode = (node) => {
      if (!node || !node.parentElement || shouldSkipLocalTranslation(node.parentElement)) return;
      const translated = translateLocalLiteral(node.nodeValue);
      if (translated !== node.nodeValue) {
        node.nodeValue = translated;
      }
    };

    if (target.nodeType === Node.TEXT_NODE) {
      translateTextNode(target);
      return;
    }

    const walker = document.createTreeWalker(target, NodeFilter.SHOW_TEXT);
    let node = walker.nextNode();
    while (node) {
      translateTextNode(node);
      node = walker.nextNode();
    }

    const nodes = [];
    if (target.nodeType === Node.ELEMENT_NODE) nodes.push(target);
    if (target.querySelectorAll) nodes.push(...target.querySelectorAll("[title],[placeholder],[aria-label]"));
    nodes.forEach((el) => {
      if (shouldSkipLocalTranslation(el)) return;
      ["title", "placeholder", "aria-label"].forEach((attr) => {
        if (!el.hasAttribute(attr)) return;
        const current = el.getAttribute(attr) || "";
        const translated = translateLocalLiteral(current);
        if (translated !== current) {
          el.setAttribute(attr, translated);
        }
      });
    });
  }

  function createGuidedActions() {
    return {
      refreshed: false,
      generated: false,
      selectedAdapter: false,
      hotspotAuto: false,
      hotspotStarted: false,
      serverStarted: false,
    };
  }

  function debugNow() {
    const now = new Date();
    const locale = window.LanShareI18n && typeof window.LanShareI18n.getLocale === "function"
      ? window.LanShareI18n.getLocale()
      : "en";
    return now.toLocaleTimeString(locale, { hour12: false });
  }

  function formatDebugExtra(extra) {
    if (extra === undefined || extra === null || extra === "") {
      return "";
    }
    if (typeof extra === "string") {
      return extra;
    }
    try {
      return JSON.stringify(extra);
    } catch {
      return String(extra);
    }
  }

  function debugLog(message, extra) {
    const suffix = formatDebugExtra(extra);
    const line = "[" + debugNow() + "] " + message + (suffix ? " | " + suffix : "");
    debugState.entries.push(line);
    if (debugState.entries.length > debugState.maxEntries) {
      debugState.entries.splice(0, debugState.entries.length - debugState.maxEntries);
    }
    const logNode = $("shareDebugLog");
    if (logNode) {
      logNode.textContent = debugState.entries.join("\n");
    }
    if (window.console && typeof window.console.log === "function") {
      window.console.log("[simple-share]", line);
    }
  }

  function emitDomProbe(reason) {
    if (!window.chrome || !window.chrome.webview || typeof window.chrome.webview.postMessage !== "function") {
      return;
    }
    if (debugState.domProbeCount >= 6) {
      return;
    }
    debugState.domProbeCount += 1;
    try {
      const shell = document.querySelector(".shell");
      const activeRoute = Array.from(document.querySelectorAll("[data-route-view]")).find((node) => !node.hidden);
      const shellStyle = shell ? window.getComputedStyle(shell) : null;
      const bodyStyle = document.body ? window.getComputedStyle(document.body) : null;
      const text = shell ? shell.innerText.trim() : (document.body ? document.body.innerText.trim() : "");
      window.chrome.webview.postMessage(JSON.stringify({
        source: "admin-shell-debug",
        kind: "dom-probe",
        reason,
        readyState: document.readyState || "",
        route: activeRoute ? activeRoute.getAttribute("data-route-view") : "",
        shellPresent: !!shell,
        shellDisplay: shellStyle ? shellStyle.display : "",
        shellVisibility: shellStyle ? shellStyle.visibility : "",
        shellOpacity: shellStyle ? shellStyle.opacity : "",
        bodyBackgroundColor: bodyStyle ? bodyStyle.backgroundColor : "",
        textLength: text.length,
        textSample: text.slice(0, 180),
      }));
    } catch (error) {
      window.chrome.webview.postMessage(JSON.stringify({
        source: "admin-shell-debug",
        kind: "dom-probe-error",
        reason,
        detail: error ? String(error) : "unknown",
      }));
    }
  }

  function $(id) {
    return document.getElementById(id);
  }

  function setText(id, text) {
    const node = $(id);
    if (node) {
      node.textContent = translateLocalLiteral(text);
    }
  }

  function setHtml(id, html) {
    const node = $(id);
    if (node) {
      node.innerHTML = html;
      applyLocalTranslations(node);
    }
  }

  function setStatusBadge(id, label, tone) {
    const node = $(id);
    if (!node) return;
    node.textContent = translateLocalLiteral(label);
    node.className = tone ? "status " + tone : "status";
  }

  function send(message) {
    if (!window.chrome || !window.chrome.webview || typeof window.chrome.webview.postMessage !== "function") {
      debugLog("native bridge unavailable", message && message.kind ? message.kind : "");
      setText("bridgeStatus", localMessage("bridge_unavailable"));
      return;
    }
    if (message && (message.kind === "command" || message.kind === "request-snapshot" || message.kind === "ready")) {
      debugLog("send native", message);
    }
    window.chrome.webview.postMessage(JSON.stringify({ source: "admin-shell", ...message }));
  }

  function setPairs(id, entries) {
    const root = $(id);
    if (!root) return;
    root.innerHTML = entries.map(([label, value]) => {
      const safeValue = value === undefined || value === null || value === "" ? "-" : String(value);
      return "<div class=\"row\"><dt>" + label + "</dt><dd>" + safeValue + "</dd></div>";
    }).join("");
    applyLocalTranslations(root);
  }

  function normalizeTab(tab) {
    const value = String(tab || "").toLowerCase();
    return ["dashboard", "session", "network", "sharing", "monitor", "diagnostics", "settings"].includes(value)
      ? value
      : "dashboard";
  }

  function normalizeRoute(route) {
    if (route === "last-simple") {
      return lastSimpleRoute || "guide";
    }
    const value = String(route || "").toLowerCase();
    return ["guide", "prepare", "share", "advanced"].includes(value) ? value : "guide";
  }

  function switchRoute(route) {
    const normalized = normalizeRoute(route);
    currentRoute = normalized;
    if (normalized !== "advanced") {
      lastSimpleRoute = normalized;
    }
    document.querySelectorAll("[data-route-view]").forEach((node) => {
      const active = node.getAttribute("data-route-view") === normalized;
      node.hidden = !active;
    });
  }

  function requestRender() {
    render(state);
  }

  function currentPrepareMode() {
    if (guidedState.activeMode) return guidedState.activeMode;
    if (guidedState.choiceMode === "hotspot") return "hotspot";
    return "lan";
  }

  function hasUsableHostIp(payload) {
    const value = String(payload.hostIp || "");
    return !!value && value !== "(not found)" && value !== "0.0.0.0";
  }

  function hostStateValue(payload) {
    return String(payload.hostState || "").toLowerCase();
  }

  function captureStateValue(payload) {
    return String(payload.captureState || "").toLowerCase();
  }

  function captureLabelValue(payload) {
    return String(payload.captureLabel || "").trim();
  }

  function isCaptureSelecting(payload) {
    const value = captureStateValue(payload);
    return value === "selecting" || hostStateValue(payload) === "starting_share";
  }

  function hasActiveCapture(payload) {
    const value = captureStateValue(payload);
    return value === "active" || value === "sharing";
  }

  function isHostSharing(payload) {
    if (hasActiveCapture(payload)) {
      return true;
    }
    const value = hostStateValue(payload);
    return value === "sharing" || value === "shared" || value === "streaming";
  }

  function baseServiceReady(payload) {
    return !!payload.serverRunning && !!payload.healthReady;
  }

  function prepareReadyForMode(mode, payload) {
    if (mode === "hotspot") {
      return baseServiceReady(payload) && (!!payload.hotspotRunning || !!payload.hostReachable);
    }
    return baseServiceReady(payload) && !!payload.hostReachable;
  }

  function detectConnectionPath(payload) {
    if (payload.hotspotRunning) return "本机热点";
    if (String(payload.networkMode || "").toLowerCase() === "hotspot") return "本机热点";
    return "同一网络";
  }

  function yesNo(value, yesLabel, noLabel) {
    return value ? yesLabel : noLabel;
  }

  function makeIssue(key, title, detail, actionLabel, action) {
    return { key, title, detail, actionLabel, action };
  }

  function resetGuidedFlow(choiceMode) {
    guidedState.choiceMode = choiceMode || "";
    guidedState.activeMode = choiceMode === "hotspot" ? "hotspot" : "lan";
    guidedState.startedAt = Date.now();
    guidedState.lastCommandAt = 0;
    guidedState.status = choiceMode ? "working" : "idle";
    guidedState.notice = choiceMode === "auto"
      ? localMessage("auto_detect_notice")
      : "";
    guidedState.issue = null;
    guidedState.actions = createGuidedActions();
    guidedState.pendingHostOpen = false;
    guidedState.hostWindowHint = "";
    guidedState.justStopped = false;
  }

  function startGuidedFlow(choiceMode) {
    resetGuidedFlow(choiceMode);
    switchRoute("prepare");
    requestRender();
  }

  function guidedModeLabel(mode) {
    if (mode === "hotspot") return "本机热点";
    if (mode === "auto") return "自动判断";
    return "同一网络";
  }

  function guidedActiveModeLabel() {
    const mode = currentPrepareMode();
    if (guidedState.choiceMode === "auto" && mode === "hotspot") {
      return "自动判断，当前已切换为本机热点";
    }
    if (guidedState.choiceMode === "auto") {
      return "自动判断，当前正在尝试同网共享";
    }
    return guidedModeLabel(mode);
  }

  function recommendedCandidateIndex(payload) {
    const candidates = payload.networkCandidates || [];
    if (!candidates.length) return -1;
    const recommended = candidates.findIndex((item) => item.recommended);
    if (recommended >= 0) return recommended;
    if (candidates.length === 1) return 0;
    return -1;
  }

  function canSendGuidedCommand() {
    return Date.now() - guidedState.lastCommandAt > 500;
  }

  function markGuidedCommandSent() {
    guidedState.lastCommandAt = Date.now();
  }

  function shouldFallbackToHotspot(payload, elapsedMs) {
    if (guidedState.choiceMode !== "auto") return false;
    if (currentPrepareMode() !== "lan") return false;
    if (payload.hotspotRunning) return false;
    if (elapsedMs < 2400) return false;

    const noLanPath = !hasUsableHostIp(payload) && Number(payload.activeIpv4Candidates || 0) === 0;
    const lanProbeFailed = payload.serverRunning && payload.healthReady && !payload.hostReachable && elapsedMs > 5200;
    return noLanPath || lanProbeFailed;
  }

  function activateHotspotFallback() {
    guidedState.activeMode = "hotspot";
    guidedState.notice = localMessage("switched_to_hotspot_notice");
    guidedState.issue = null;
    guidedState.status = "working";
    guidedState.actions.hotspotAuto = false;
    guidedState.actions.hotspotStarted = false;
    guidedState.lastCommandAt = 0;
    requestRender();
  }

  function computeGuidedIssue(payload, mode, elapsedMs) {
    if (mode === "lan" && elapsedMs > 5200 && !hasUsableHostIp(payload) && Number(payload.activeIpv4Candidates || 0) === 0) {
      return makeIssue(
        "no-network",
        "未检测到可用网络",
        "当前没有找到适合同网共享的地址。请确认电脑已连到网络，或者改用本机热点。",
        "改用本机热点",
        "switch-hotspot"
      );
    }

    if (mode === "lan" && payload.serverRunning && elapsedMs > 7800 && !payload.hostReachable) {
      return makeIssue(
        "lan-path",
        "当前同网路径还不可用",
        payload.remoteViewerDetail || "系统已经启动共享服务，但当前局域网地址还不能稳定用于交付。",
        "重新检测网络",
        "refresh-network"
      );
    }

    if (mode === "hotspot" && guidedState.actions.hotspotStarted && elapsedMs > 7600 && !payload.hotspotRunning) {
      if (!payload.hotspotSupported || String(payload.hotspotStatus || "").toLowerCase().includes("system settings required")) {
        return makeIssue(
          "hotspot-settings",
          "当前机器无法直接启动热点",
          "这台电脑需要通过 Windows 系统设置来开启热点。",
          "打开系统热点设置",
          "open-hotspot-settings"
        );
      }
      return makeIssue(
        "hotspot-start",
        "热点启动失败",
        "系统尝试启动热点但没有成功。你可以重试，或者改用系统热点设置继续。",
        "重试启动热点",
        "retry-hotspot"
      );
    }

    if (guidedState.actions.serverStarted && elapsedMs > 8200 && !payload.serverRunning) {
      return makeIssue(
        "service-start",
        "共享服务启动失败",
        payload.lastError || payload.dashboardError || "系统未能启动本地共享服务。",
        "重新启动服务",
        "retry-service"
      );
    }

    if (payload.serverRunning && elapsedMs > 8200 && !payload.healthReady) {
      return makeIssue(
        "service-health",
        "共享服务还未准备好",
        payload.dashboardError || "服务已经启动，但健康检查还没有通过。",
        "刷新状态",
        "request-snapshot"
      );
    }

    return null;
  }

  function runGuidedPreparation(payload) {
    if (currentRoute !== "prepare" || !guidedState.choiceMode) return;

    const elapsedMs = Date.now() - guidedState.startedAt;
    const mode = currentPrepareMode();

    if (shouldFallbackToHotspot(payload, elapsedMs)) {
      activateHotspotFallback();
      return;
    }

    if (prepareReadyForMode(mode, payload)) {
      guidedState.status = "ready";
      guidedState.issue = null;
      switchRoute("share");
      requestRender();
      return;
    }

    const issue = computeGuidedIssue(payload, mode, elapsedMs);
    if (issue) {
      guidedState.status = "blocked";
      guidedState.issue = issue;
      return;
    }

    guidedState.status = "working";
    guidedState.issue = null;
    if (!canSendGuidedCommand()) return;

    if (!guidedState.actions.refreshed) {
      handleCommand("refresh-network");
      guidedState.actions.refreshed = true;
      markGuidedCommandSent();
      return;
    }

    if (!guidedState.actions.generated) {
      handleCommand("generate-room-token");
      guidedState.actions.generated = true;
      markGuidedCommandSent();
      return;
    }

    const preferredIndex = recommendedCandidateIndex(payload);
    if (preferredIndex >= 0 &&
        payload.networkCandidates &&
        payload.networkCandidates[preferredIndex] &&
        !payload.networkCandidates[preferredIndex].selected &&
        !guidedState.actions.selectedAdapter) {
      handleCommand("select-adapter", { index: preferredIndex });
      guidedState.actions.selectedAdapter = true;
      markGuidedCommandSent();
      return;
    }

    if (mode === "hotspot" && !guidedState.actions.hotspotAuto) {
      handleCommand("auto-hotspot");
      guidedState.actions.hotspotAuto = true;
      markGuidedCommandSent();
      return;
    }

    if (mode === "hotspot" && !payload.hotspotRunning && !guidedState.actions.hotspotStarted) {
      handleCommand("start-hotspot");
      guidedState.actions.hotspotStarted = true;
      markGuidedCommandSent();
      return;
    }

    if (!payload.serverRunning && !guidedState.actions.serverStarted) {
      handleCommand("start-server");
      guidedState.actions.serverStarted = true;
      markGuidedCommandSent();
    }
  }

  function setPreviewBadge(label, tone) {
    const badge = $("hostPreviewBadge");
    if (!badge) return;
    badge.textContent = label;
    badge.className = "status status-inline " + tone;
  }

  function updateHostPreview(payload) {
    const frame = $("hostPreviewFrame");
    const empty = $("hostPreviewEmpty");
    const title = $("hostPreviewEmptyTitle");
    const body = $("hostPreviewEmptyBody");
    const note = $("hostPreviewNote");
    if (!frame || !empty || !title || !body || !note) return;

    const hostState = String(payload.hostState || "").toLowerCase();
    const hostUrl = payload.serverRunning ? String(payload.hostUrl || "") : "";

    if (!hostUrl) {
      if (previewUrl) {
        frame.src = "about:blank";
      }
      previewUrl = "";
      previewLoaded = false;
      hostBridge.ready = false;
      empty.hidden = false;
      title.textContent = "Host Preview Unavailable";
      body.textContent = payload.serverRunning
        ? "The service is up, but the Host URL is still empty. Apply session values and refresh the snapshot."
        : "Start the local service first, then the embedded Host page can load inside this panel.";
      note.textContent = "If the embedded preview is not available yet, open Host in the system browser and continue sharing there.";
      setPreviewBadge(payload.serverRunning ? "Waiting For URL" : "Idle", payload.serverRunning ? "warn" : "idle");
      return;
    }

    if (previewUrl !== hostUrl) {
      previewUrl = hostUrl;
      previewLoaded = false;
      hostBridge.ready = false;
      frame.src = hostUrl;
    }

    note.textContent = payload.hostReachable
      ? "The embedded preview is bound to the same Host URL that will be handed to the operator."
      : "The Host page can load locally, but LAN reachability still needs attention before handing off Viewer access.";

    if (!previewLoaded) {
      empty.hidden = false;
      title.textContent = "Loading Host Preview";
      body.textContent = "The embedded /host page is loading inside the local admin shell. If it stays blank, wait a moment and refresh the current snapshot.";
      setPreviewBadge("Loading", "idle");
      return;
    }

    empty.hidden = true;
    if (hostState === "sharing") {
      setPreviewBadge("Sharing", "ok");
    } else if (hostState === "ready") {
      setPreviewBadge("Ready", "ok");
    } else if (hostState === "loading") {
      setPreviewBadge("Loading", "idle");
    } else if (payload.serverRunning) {
      setPreviewBadge("Attention", "warn");
    } else {
      setPreviewBadge("Idle", "idle");
    }
  }

  function nextHostBridgeRequestId() {
    hostBridge.requestSeq += 1;
    return "host-bridge-" + hostBridge.requestSeq;
  }

  function clearHostBridgeTimeout() {
    if (hostBridge.timeoutHandle) {
      window.clearTimeout(hostBridge.timeoutHandle);
      hostBridge.timeoutHandle = 0;
    }
  }

  function mergeHostBridgePatch(patch) {
    if (!patch || typeof patch !== "object") return;
    Object.assign(state, patch);
    syncGuidedShareFlags(state);
    renderSimpleMode(state);
    renderAdvancedShell(state);
  }

  function hostBridgeRequestMatches(requestId) {
    return !!hostBridge.pendingCommand && hostBridge.pendingCommand.requestId === requestId;
  }

  function finalizeHostBridgeRequest(requestId) {
    if (hostBridge.inFlightRequestId === requestId) {
      hostBridge.inFlightRequestId = "";
    }
    if (hostBridgeRequestMatches(requestId)) {
      hostBridge.pendingCommand = null;
    }
    clearHostBridgeTimeout();
  }

  function hasPendingHostBridgeCommand() {
    return !!hostBridge.pendingCommand || !!hostBridge.inFlightRequestId;
  }

  function fallbackOpenHostWindow(command) {
    guidedState.pendingHostOpen = false;
    guidedState.hostWindowHint = localMessage(command === "choose-share"
      ? "embedded_share_page_fallback_pick"
      : "embedded_share_page_fallback_start");
    requestRender();
  }

  function handleQueuedHostBridgeTimeout(requestId) {
    if (hostBridge.inFlightRequestId === requestId) {
      return;
    }
    const pending = hostBridge.pendingCommand;
    if (!pending || pending.requestId !== requestId) {
      return;
    }
    debugLog("queued host bridge timeout", { requestId, command: pending.command });
    hostBridge.pendingCommand = null;
    clearHostBridgeTimeout();
    if (pending.stopServerAfter) {
      guidedState.pendingHostOpen = false;
      guidedState.hostWindowHint = localMessage("share_page_unresponsive_closed");
      handleCommand("stop-server");
      requestRender();
      return;
    }
    fallbackOpenHostWindow(pending.command);
  }

  function handleHostBridgeTimeout(requestId) {
    if (hostBridge.inFlightRequestId !== requestId) return;
    const pending = hostBridge.pendingCommand;
    hostBridge.inFlightRequestId = "";
    clearHostBridgeTimeout();
    if (!pending || pending.requestId !== requestId) {
      return;
    }
    debugLog("in-flight host bridge timeout", { requestId, command: pending.command });
    hostBridge.pendingCommand = null;
    if (pending.stopServerAfter) {
      guidedState.pendingHostOpen = false;
      guidedState.hostWindowHint = localMessage("finishing_share");
      handleCommand("stop-server");
      requestRender();
      return;
    }
    fallbackOpenHostWindow(pending.command);
  }

  function tryFlushHostBridgeCommand() {
    const pending = hostBridge.pendingCommand;
    if (!pending || hostBridge.inFlightRequestId) return false;

    const frame = $("hostPreviewFrame");
    let blockedReason = "";
    if (!frame || !frame.contentWindow) {
      blockedReason = "frame-missing";
    } else if (!previewLoaded) {
      blockedReason = "preview-not-loaded";
    } else if (!hostBridge.ready) {
      blockedReason = "host-bridge-not-ready";
    } else if (!state.hostUrl) {
      blockedReason = "host-url-empty";
    }
    if (blockedReason) {
      if (hostBridge.lastBlockedReason !== blockedReason) {
        hostBridge.lastBlockedReason = blockedReason;
        debugLog("host bridge blocked", {
          blockedReason,
          requestId: pending.requestId,
          command: pending.command,
          previewLoaded,
          hostReady: hostBridge.ready,
          hostUrl: state.hostUrl || "",
        });
      }
      return false;
    }
    hostBridge.lastBlockedReason = "";

    hostBridge.inFlightRequestId = pending.requestId;
    clearHostBridgeTimeout();
    debugLog("flush host bridge command", { requestId: pending.requestId, command: pending.command });
    hostBridge.timeoutHandle = window.setTimeout(() => {
      handleHostBridgeTimeout(pending.requestId);
    }, pending.timeoutMs || 9000);

    frame.contentWindow.postMessage({
      source: "lan-share-admin",
      kind: "host-control",
      requestId: pending.requestId,
      command: pending.command,
    }, "*");
    return true;
  }

  function queueHostBridgeCommand(command, options) {
    if (hostBridge.pendingCommand || hostBridge.inFlightRequestId) {
      debugLog("host bridge already busy", {
        command,
        pendingCommand: hostBridge.pendingCommand ? hostBridge.pendingCommand.command : "",
        inFlightRequestId: hostBridge.inFlightRequestId,
      });
      return hostBridge.pendingCommand ? hostBridge.pendingCommand.requestId : hostBridge.inFlightRequestId;
    }
    const pending = {
      requestId: nextHostBridgeRequestId(),
      command,
      stopServerAfter: !!(options && options.stopServerAfter),
      timeoutMs: options && options.timeoutMs ? options.timeoutMs : 9000,
      queueTimeoutMs: options && options.queueTimeoutMs ? options.queueTimeoutMs : (state.serverRunning ? 4500 : 12000),
    };
    hostBridge.pendingCommand = pending;
    guidedState.pendingHostOpen = true;
    debugLog("queue host bridge command", pending);
    clearHostBridgeTimeout();
    hostBridge.timeoutHandle = window.setTimeout(() => {
      handleQueuedHostBridgeTimeout(pending.requestId);
    }, pending.queueTimeoutMs);
    tryFlushHostBridgeCommand();
    return pending.requestId;
  }

  function startGuidedShareCommand(command) {
    debugLog("startGuidedShareCommand", {
      command,
      serverRunning: state.serverRunning,
      hostUrl: state.hostUrl || "",
      previewLoaded,
      hostReady: hostBridge.ready,
    });
    guidedState.justStopped = false;
    const starting = command === "choose-share"
      ? localMessage("opening_share_picker_system")
      : localMessage("starting_share_system");
    const waiting = state.serverRunning
      ? starting
      : localMessage("preparing_environment_auto_share");
    guidedState.hostWindowHint = waiting;

    if (!state.serverRunning) {
      handleCommand("start-server");
    }

    queueHostBridgeCommand(command);
    switchRoute("share");
    requestRender();
  }

  function stopGuidedShareCommand() {
    debugLog("stopGuidedShareCommand", {
      serverRunning: state.serverRunning,
      hostState: state.hostState || "",
      captureState: state.captureState || "",
    });
    guidedState.pendingHostOpen = false;
    guidedState.justStopped = true;
    guidedState.hostWindowHint = localMessage("finishing_share");

    if (isHostSharing(state) || isCaptureSelecting(state)) {
      queueHostBridgeCommand("stop-share", { stopServerAfter: true, timeoutMs: 5000 });
    } else {
      handleCommand("stop-server");
    }
    requestRender();
  }

  function switchTab(tab, notifyNative) {
    const normalizedTab = normalizeTab(tab);
    const changed = activeTab !== normalizedTab;
    activeTab = normalizedTab;
    document.querySelectorAll(".tab").forEach((button) => {
      button.classList.toggle("active", button.getAttribute("data-tab") === normalizedTab);
    });
    document.querySelectorAll(".view").forEach((view) => {
      view.classList.toggle("active", view.getAttribute("data-view") === normalizedTab);
    });
    if (notifyNative && changed) {
      send({ kind: "command", command: "switch-page", page: normalizedTab });
    }
  }

  function dashboardSuggestions(payload) {
    const items = [];
    if (!payload.serverRunning) {
      items.push(["Start the local service", "Use Start Sharing to launch the local HTTP/WS service before handing out links."]);
    }
    if (!payload.hostIp || payload.hostIp === "(not found)") {
      items.push(["Select a usable LAN address", "No primary host IP is selected yet. Refresh network detection or pick another adapter."]);
    }
    if (payload.serverRunning && !payload.healthReady) {
      items.push(["Investigate /health", "The process is running, but the local health probe is still not healthy."]);
    }
    if (payload.serverRunning && !payload.hostReachable) {
      items.push(["Check adapter selection", "The selected host address is not responding to the reachability probe."]);
    }
    if (payload.serverRunning && !payload.firewallReady) {
      items.push(["Review inbound firewall policy", payload.firewallDetail || "Windows Firewall does not yet show a clear inbound allow path for the current viewer entry."]);
    }
    if (payload.serverRunning && !payload.remoteViewerReady) {
      items.push(["Validate remote viewer reachability", payload.remoteViewerDetail || "The current Viewer URL is not yet validated for another device on the LAN."]);
    }
    if (!payload.shareBundleExported) {
      items.push(["Refresh offline materials", "The share bundle has not been exported for this session yet."]);
    }
    if (items.length === 0) {
      items.push(["Ready for handoff", "The current session looks healthy enough for the operator to continue sharing."]);
    }
    return items;
  }

  function diagnosticsChecklist(payload) {
    return [
      ["Port listening", payload.canStartSharing ? "Ready or blocked by sharing state" : "Sharing active"],
      ["Local /health", payload.healthReady ? "OK" : "Needs attention"],
      ["Selected host IP", payload.hostReachable ? "Reachable" : "Not reachable yet"],
      ["Firewall inbound path", payload.firewallReady ? "Ready" : (payload.firewallDetail || "Needs attention")],
      ["Remote viewer path", payload.remoteViewerReady ? "Ready" : (payload.remoteViewerDetail || "Needs attention")],
      ["Bundle export", payload.shareBundleExported ? "Exported" : "Not exported"],
      ["WebView runtime", payload.webviewStatus || "Unknown"]
    ];
  }

  function sharingGuide(payload) {
    return [
      ["Same LAN", "Keep the viewer device on the same router or switch as the host and open the Viewer URL."],
      ["Hotspot mode", payload.hotspotRunning ? "Host hotspot is active. Join the SSID shown in Network, then open the Viewer URL." : "If no shared LAN is available, start hotspot in the Network tab first."],
      ["Local access", "The local admin and host pages now run over plain HTTP on this machine. The viewer only needs the ViewMesh Viewer URL."],
      ["Firewall", payload.firewallReady ? "Firewall looks compatible with inbound viewer traffic on this machine." : (payload.firewallDetail || "Open Windows Firewall settings and confirm there is an inbound allow rule for the current server path or port.")],
      ["Common failure", payload.remoteViewerReady ? "If a viewer still fails, test the Viewer URL directly in a browser." : (payload.remoteViewerDetail || "If viewers fail, re-check adapter selection, firewall policy, and same-LAN reachability first.")]
    ];
  }

  function renderSuggestions(id, items) {
    setHtml(id, items.map(([title, detail]) => {
      return "<article class=\"suggestion\"><h3>" + title + "</h3><p>" + detail + "</p></article>";
    }).join(""));
  }

  function applySessionForm(payload) {
    if (sessionDirty) {
      return;
    }
    if ($("roomInput")) $("roomInput").value = payload.room || "";
    if ($("tokenInput")) $("tokenInput").value = payload.token || "";
    if ($("bindInput")) $("bindInput").value = payload.bind || "";
    if ($("portInput")) $("portInput").value = payload.port || 9443;
  }

  function applyHotspotForm(payload) {
    if (hotspotDirty) {
      return;
    }
    if ($("hotspotSsidInput")) $("hotspotSsidInput").value = payload.hotspotSsid || "";
    if ($("hotspotPasswordInput")) $("hotspotPasswordInput").value = payload.hotspotPassword || "";
  }

  function renderCandidates(payload) {
    const root = $("adapterList");
    if (!root) return;
    const candidates = payload.networkCandidates || [];
    if (!candidates.length) {
      root.innerHTML = "<div class=\"empty\">No adapter candidates were detected.</div>";
      return;
    }
    root.innerHTML = candidates.map((item, index) => {
      const flags = [];
      if (item.recommended) flags.push("Recommended");
      if (item.selected) flags.push("Selected");
      return "<article class=\"candidate\">" +
        "<div><h3>" + item.name + "</h3><p>" + item.ip + " | " + item.type + "</p><div class=\"chip-row\">" +
        flags.map((flag) => "<span class=\"chip\">" + flag + "</span>").join("") +
        "</div></div>" +
        "<button class=\"secondary\" data-command=\"select-adapter\" data-index=\"" + index + "\">Use As Main</button>" +
        "</article>";
    }).join("");
  }

  function renderMetrics(payload) {
    const metrics = [
      ["Rooms", payload.rooms],
      ["Viewers", payload.viewers],
      ["Host State", payload.hostState],
      ["/health", payload.healthReady ? "OK" : "ATTN"],
      ["Reachability", payload.hostReachable ? "OK" : "ATTN"]
    ];
    setHtml("monitorMetrics", metrics.map(([label, value]) => {
      return "<article class=\"metric\"><span>" + label + "</span><strong>" + value + "</strong></article>";
    }).join(""));
  }

  function renderChecklist(payload) {
    setHtml("diagChecklist", diagnosticsChecklist(payload).map(([label, value]) => {
      return "<div class=\"checkitem\"><strong>" + label + "</strong><span>" + value + "</span></div>";
    }).join(""));
  }

  function handoffSummary(payload) {
    const stateName = String(payload.handoffState || "").toLowerCase();
    const stateLabel = payload.handoffLabel || "Not started";
    const stateDetail = payload.handoffDetail || "Open Share Wizard or copy the Viewer URL when you are ready to hand off the session.";
    return [
      ["State", stateLabel],
      ["Share Wizard", payload.shareWizardOpened ? "Opened" : "Not opened yet"],
      ["Handoff started", payload.handoffStarted ? "Yes" : "No"],
      ["Viewer connected", payload.handoffDelivered ? "Yes" : "No"],
      ["Next step", stateName === "delivered" ? "Keep sharing or monitor the session." : stateDetail]
    ];
  }

  function quickFixItems(payload) {
    const items = [];
    if (!payload.serverRunning) {
      items.push(["Service is not running", "Start the local share service again before handing off access.", "quick-fix-sharing", "Start sharing"]);
    }
    if (!payload.hostIp || payload.hostIp === "(not found)" || !payload.hostReachable) {
      items.push(["LAN endpoint still needs attention", "Refresh adapter detection and re-check which address should be used as the main viewer entry.", "quick-fix-network", "Refresh network"]);
    }
    if (payload.webviewStatus === "runtime-unavailable" || payload.webviewStatus === "controller-unavailable") {
      items.push(["Embedded admin preview needs WebView2 runtime attention", "Run the runtime helper or install/repair Evergreen WebView2 Runtime, then reopen the admin shell.", "check-webview-runtime", "Check WebView2 runtime"]);
    }
    if (payload.serverRunning && !payload.firewallReady) {
      items.push(["Firewall inbound path still needs attention", payload.firewallDetail || "Open Windows Firewall settings and confirm an inbound allow rule exists for the current share path.", "open-firewall-settings", "Open firewall settings"]);
    }
    if (payload.serverRunning && !payload.remoteViewerReady) {
      items.push(["Remote viewer path still needs validation", payload.remoteViewerDetail || "Collect a local network diagnostics report before retrying from another device.", "run-network-diagnostics", "Run diagnostics"]);
      if (payload.remoteProbeAction) {
        items.push(["Prepare a remote-device test guide", payload.remoteProbeAction, "export-remote-probe-guide", "Export guide"]);
      }
    }
    if ((payload.shareWizardOpened || payload.handoffStarted) && !payload.handoffDelivered && payload.serverRunning && payload.healthReady && payload.hostReachable) {
      items.push(["Viewer handoff material is ready", "Copy the Viewer URL again or show the QR / share card while the other device connects.", "quick-fix-handoff", "Show QR + copy link"]);
    }
    if (!payload.hotspotRunning && (!payload.hostReachable || Number(payload.activeIpv4Candidates || 0) === 0)) {
      items.push(["Fallback path may be needed", "If the current LAN path is unstable, open or start hotspot before retrying the viewer handoff.", "quick-fix-hotspot", "Open hotspot path"]);
    }
    if (!items.length) {
      items.push(["No blocking issue detected", "The current session looks healthy. Open Share Wizard or keep monitoring viewer activity.", "show-share-wizard", "Open Share Wizard"]);
    }
    return items.slice(0, 4);
  }

  function renderQuickFixes(payload) {
    setHtml("dashboardQuickFixes", quickFixItems(payload).map(([title, detail, command, label]) => {
      return "<article class=\"quick-fix\"><div><h3>" + title + "</h3><p>" + detail + "</p></div><button class=\"secondary\" data-command=\"" + command + "\">" + label + "</button></article>";
    }).join(""));
  }

  function updateRouteFromSnapshot(payload) {
    if (guidedState.initializedFromSnapshot) return;
    if (!Object.prototype.hasOwnProperty.call(payload || {}, "serverRunning")) return;
    if (payload.serverRunning || isHostSharing(payload) || Number(payload.viewers || 0) > 0) {
      switchRoute("share");
    } else {
      switchRoute("guide");
    }
    guidedState.initializedFromSnapshot = true;
  }

  function syncGuidedShareFlags(payload) {
    if (isCaptureSelecting(payload)) {
      guidedState.pendingHostOpen = true;
      guidedState.justStopped = false;
      guidedState.hostWindowHint = localMessage("share_picker_system_detail");
      return;
    }

    if (isHostSharing(payload)) {
      guidedState.pendingHostOpen = false;
      guidedState.justStopped = false;
      guidedState.hostWindowHint = Number(payload.viewers || 0) > 0
        ? localMessage("viewer_connected_keep_sharing")
        : localMessage("sharing_waiting_viewer");
      return;
    }

    if (!payload.serverRunning && guidedState.justStopped) {
      guidedState.hostWindowHint = localMessage("share_stopped_back_previous");
      return;
    }

    if (!payload.serverRunning && !guidedState.pendingHostOpen) {
      guidedState.hostWindowHint = "";
    }
  }

  function renderGuide(payload) {
    const guideBadge = payload.serverRunning
      ? "共享服务可继续"
      : payload.hotspotRunning
      ? "热点已就绪"
      : hasUsableHostIp(payload)
      ? "已检测到网络"
      : "等待检测";
    const guideTone = payload.serverRunning || payload.hotspotRunning || hasUsableHostIp(payload) ? "ok" : "idle";
    setStatusBadge("guideBridgeBadge", guideBadge, guideTone);

    setPairs("guideEnvironmentCard", [
      ["当前路径", detectConnectionPath(payload)],
      ["共享地址", payload.hostIp || "(未检测到)"],
      ["热点控制", payload.hotspotSupported ? "可直接启动" : "需系统设置"],
      ["热点状态", payload.hotspotStatus || "stopped"]
    ]);

    const sessionState = isHostSharing(payload)
      ? "正在共享"
      : payload.serverRunning
      ? "服务已启动"
      : "尚未开始";

    setPairs("guideSessionCard", [
      ["当前状态", sessionState],
      ["连接人数", payload.viewers],
      ["访问地址", payload.viewerUrl || "准备后自动生成"],
      ["最近交付", payload.handoffLabel || "未开始"]
    ]);

    const resumeBtn = $("guideResumeBtn");
    if (resumeBtn) {
      resumeBtn.disabled = !(payload.serverRunning || isHostSharing(payload) || Number(payload.viewers || 0) > 0);
    }
  }

  function buildPrepareSteps(payload) {
    const mode = currentPrepareMode();
    const ready = prepareReadyForMode(mode, payload);

    if (mode === "hotspot") {
      return [
        {
          label: "正在生成热点信息",
          detail: "系统正在准备热点名称和密码。",
          complete: !!payload.hotspotSsid && !!payload.hotspotPassword,
        },
        {
          label: "正在启动热点",
          detail: "接收方需要先连接这个热点。",
          complete: !!payload.hotspotRunning,
        },
        {
          label: "正在启动共享服务",
          detail: "本地共享服务启动后才能生成访问入口。",
          complete: !!payload.serverRunning,
        },
        {
          label: "正在生成连接信息",
          detail: "系统会准备访问地址与二维码。",
          complete: !!payload.viewerUrl,
        },
        {
          label: "准备完成",
          detail: "可以进入共享页面，选择要共享的内容。",
          complete: ready,
        }
      ];
    }

    return [
      {
        label: "正在检测网络",
        detail: "系统正在确认当前电脑的可用网络路径。",
        complete: hasUsableHostIp(payload) || Number(payload.activeIpv4Candidates || 0) > 0,
      },
      {
        label: "正在选择共享地址",
        detail: "如有多个地址，系统会优先选推荐的那个。",
        complete: hasUsableHostIp(payload),
      },
      {
        label: "正在启动共享服务",
        detail: "本地共享服务启动后才能生成访问入口。",
        complete: !!payload.serverRunning,
      },
      {
        label: "正在生成连接信息",
        detail: "系统会准备访问地址与二维码。",
        complete: !!payload.viewerUrl,
      },
      {
        label: "准备完成",
        detail: "可以进入共享页面，选择要共享的内容。",
        complete: ready,
      }
    ];
  }

  function renderPrepareSteps(payload) {
    const root = $("prepareSteps");
    if (!root) return;
    const items = buildPrepareSteps(payload);
    const blocked = guidedState.status === "blocked";
    const currentIndex = items.findIndex((item) => !item.complete);

    root.innerHTML = items.map((item, index) => {
      const classes = ["step-item"];
      if (item.complete) {
        classes.push("is-complete");
      } else if (blocked && index === currentIndex) {
        classes.push("is-blocked");
      } else if (index === currentIndex) {
        classes.push("is-current");
      }
      return "<div class=\"" + classes.join(" ") + "\">" +
        "<div class=\"step-index\">" + (item.complete ? "OK" : String(index + 1)) + "</div>" +
        "<div class=\"step-copy\"><strong>" + item.label + "</strong><span>" + item.detail + "</span></div>" +
        "</div>";
    }).join("");
  }

  function renderPrepare(payload) {
    const mode = currentPrepareMode();
    const ready = prepareReadyForMode(mode, payload);
    const title = ready
      ? "准备完成，即将进入共享页面"
      : guidedState.status === "blocked"
      ? "当前准备过程被阻塞"
      : mode === "hotspot"
      ? "正在准备本机热点与共享服务"
      : "正在准备同网共享环境";

    const detail = ready
      ? "系统已经准备好当前共享环境。你可以开始选择共享内容了。"
      : guidedState.issue
      ? guidedState.issue.detail
      : mode === "hotspot"
      ? "请稍候，系统会先准备热点信息，再启动本地共享服务。"
      : "请稍候，系统会自动选择共享地址，并启动本地共享服务。";

    setText("prepareModeBadgeText", guidedActiveModeLabel());
    setText("prepareTitle", title);
    setText("prepareDetail", detail);
    setText("prepareModeNote", guidedState.notice || "系统会尽量减少手动操作，只在确实需要你介入时才停下来。");

    if (guidedState.status === "blocked") {
      setStatusBadge("prepareBadge", "需要处理", "warn");
    } else if (ready) {
      setStatusBadge("prepareBadge", "已准备", "ok");
    } else {
      setStatusBadge("prepareBadge", "处理中", "idle");
    }

    renderPrepareSteps(payload);

    setPairs("prepareConnectionCard", [
      ["当前路径", guidedActiveModeLabel()],
      ["共享地址", payload.hostIp || "(等待检测)"],
      ["热点名称", payload.hotspotSsid || "(未使用)"],
      ["热点状态", payload.hotspotStatus || "stopped"]
    ]);

    setPairs("prepareReadinessCard", [
      ["共享服务", yesNo(payload.serverRunning, "已启动", "未启动")],
      ["本地健康", yesNo(payload.healthReady, "通过", "未通过")],
      ["访问地址", payload.viewerUrl || "(生成中)"],
      ["本机控制页", payload.hostUrl || "(生成中)"]
    ]);

    const issueCard = $("prepareIssueCard");
    if (issueCard) {
      issueCard.hidden = !guidedState.issue;
    }
    setText("prepareIssueTitle", guidedState.issue ? guidedState.issue.title : "需要处理的问题");
    setText("prepareIssueTag", guidedState.issue ? "阻塞" : "状态");
    setText("prepareIssueText", guidedState.issue ? guidedState.issue.detail : "当前没有阻塞问题。");

    const primaryBtn = $("preparePrimaryBtn");
    if (primaryBtn) {
      if (guidedState.issue) {
        primaryBtn.textContent = guidedState.issue.actionLabel;
        primaryBtn.dataset.guidedAction = guidedState.issue.action;
      } else if (ready) {
        primaryBtn.textContent = "进入共享页面";
        primaryBtn.dataset.guidedAction = "resume-share";
      } else {
        primaryBtn.textContent = "刷新状态";
        primaryBtn.dataset.guidedAction = "request-snapshot";
      }
    }

    const secondaryBtn = $("prepareSecondaryBtn");
    if (secondaryBtn) {
      secondaryBtn.textContent = "返回选择";
      secondaryBtn.dataset.guidedAction = "go-guide";
    }
  }

  function buildShareStatus(payload) {
    if (isCaptureSelecting(payload)) {
      return {
        title: localMessage("waiting_for_you"),
        detail: guidedState.hostWindowHint || localMessage("share_picker_open_detail"),
        badge: localMessage("choosing"),
        tone: "warn",
      };
    }

    if (isHostSharing(payload) && Number(payload.viewers || 0) > 0) {
      return {
        title: localMessage("viewer_connected"),
        detail: localMessage("sharing_started"),
        badge: localMessage("connected_plain"),
        tone: "ok",
      };
    }

    if (isHostSharing(payload)) {
      return {
        title: localMessage("sharing_in_progress"),
        detail: localMessage("sharing_waiting_viewer"),
        badge: localMessage("sharing_plain"),
        tone: "ok",
      };
    }

    if (guidedState.justStopped && !payload.serverRunning) {
      return {
        title: localMessage("sharing_stopped"),
        detail: localMessage("sharing_stopped_detail"),
        badge: localMessage("stopped_plain"),
        tone: "warn",
      };
    }

    if (baseServiceReady(payload)) {
      return {
        title: localMessage("waiting_content_selection"),
        detail: guidedState.hostWindowHint || localMessage("share_picker_or_window_detail"),
        badge: localMessage("ready_to_start"),
        tone: "idle",
      };
    }

    if (payload.serverRunning) {
      return {
        title: localMessage("share_one_more_step"),
        detail: payload.dashboardError || localMessage("share_service_missing_one_step"),
        badge: localMessage("repair_needed"),
        tone: "warn",
      };
    }

    return {
      title: localMessage("sharing_not_ready"),
      detail: localMessage("sharing_not_ready_detail"),
      badge: localMessage("not_ready"),
      tone: "idle",
    };
  }

  function buildShareAttention(payload) {
    if (!payload.serverRunning) {
      return {
        title: localMessage("share_service_not_ready"),
        text: localMessage("return_to_guide_then_prepare"),
        actionLabel: localMessage("back_to_guide"),
        action: "go-guide",
      };
    }

    if (!payload.hostReachable && !payload.hotspotRunning) {
      return {
        title: localMessage("address_not_ready"),
        text: payload.remoteViewerDetail || localMessage("network_not_stable_for_sharing"),
        actionLabel: localMessage("recheck_network"),
        action: "refresh-network",
      };
    }

    if (payload.serverRunning && !payload.firewallReady) {
      return {
        title: localMessage("share_one_more_step"),
        text: payload.firewallDetail || localMessage("firewall_may_block_devices"),
        actionLabel: localMessage("open_firewall_settings"),
        action: "open-firewall-settings",
      };
    }

    if (payload.serverRunning && !payload.remoteViewerReady) {
      return {
        title: localMessage("verify_access_path"),
        text: payload.remoteViewerDetail || localMessage("access_path_not_verified"),
        actionLabel: localMessage("run_network_diagnostics"),
        action: "run-network-diagnostics",
      };
    }

    return null;
  }

  function renderShare(payload) {
    const status = buildShareStatus(payload);
    const attention = buildShareAttention(payload);
    const captureLabel = captureLabelValue(payload);
    const selecting = isCaptureSelecting(payload);

    setText("shareTitle", status.title);
    setText("shareDetail", status.detail);
    setStatusBadge("shareBadge", status.badge, status.tone);
    setText(
      "shareHelperText",
      guidedState.hostWindowHint ||
      (payload.hotspotRunning
        ? localMessage("viewer_connect_hotspot_then_open")
        : localMessage("hand_address_then_share"))
    );

    const shareAttentionCard = $("shareAttentionCard");
    if (shareAttentionCard) {
      shareAttentionCard.hidden = !attention;
    }
    setText("shareAttentionTitle", attention ? attention.title : localMessage("share_one_more_step"));
    setText("shareAttentionText", attention ? attention.text : localMessage("no_extra_reminder"));

    const shareAttentionBtn = $("shareAttentionBtn");
    if (shareAttentionBtn) {
      shareAttentionBtn.dataset.guidedAction = attention ? attention.action : "request-snapshot";
      shareAttentionBtn.textContent = attention ? attention.actionLabel : localMessage("refresh_status");
    }

    setPairs("shareAccessCard", [
      [localMessage("access_link"), payload.viewerUrl || localMessage("generated_after_prepare_paren")],
      [localMessage("qr_code"), payload.viewerUrl ? localMessage("click_show_qr") : localMessage("generated_after_prepare")],
      [localMessage("hotspot_name"), payload.hotspotRunning ? (payload.hotspotSsid || localMessage("unnamed")) : localMessage("not_used_plain")],
      [localMessage("hotspot_password"), payload.hotspotRunning ? (payload.hotspotPassword || localMessage("not_generated")) : localMessage("not_used_plain")]
    ]);

    setPairs("shareMethodCard", [
      [localMessage("current_path"), detectConnectionPath(payload)],
      [localMessage("sharing_address"), payload.hostIp || localMessage("waiting_for_detection")],
      [localMessage("hotspot_status"), payload.hotspotStatus || "stopped"],
      [localMessage("local_health"), yesNo(payload.healthReady, localMessage("normal_status"), localMessage("caution_needed"))]
    ]);

    setPairs("shareSessionCard", [
      [localMessage("current_status"), isHostSharing(payload) ? localMessage("sharing_in_progress") : (payload.serverRunning ? localMessage("waiting_for_content") : localMessage("not_running_plain"))],
      [localMessage("viewers_connected"), payload.viewers],
      [localMessage("handoff_status"), payload.handoffLabel || localMessage("not_started_plain")],
      [localMessage("latest_note"), payload.handoffDetail || localMessage("next_step_after_prepare")]
    ]);

    const nextSteps = [];
    if (!isHostSharing(payload)) {
      nextSteps.push([localMessage("start_sharing_content"), localMessage("click_start_sharing_then_pick")]);
    } else if (Number(payload.viewers || 0) === 0) {
      nextSteps.push([localMessage("let_viewer_join"), payload.hotspotRunning
        ? localMessage("viewer_join_current_hotspot")
        : localMessage("viewer_stay_same_network")]);
    } else {
      nextSteps.push([localMessage("keep_sharing"), localMessage("viewer_connected_keep_sharing")]);
    }

    if (attention) {
      nextSteps.push([attention.title, attention.text]);
    } else if (payload.hotspotRunning) {
      nextSteps.push([localMessage("hotspot_info"), localMessage("hotspot_already_started_detail")]);
    } else {
      nextSteps.push([localMessage("access_method"), localMessage("prefer_link_or_qr")]);
    }
    renderSuggestions("shareNextSteps", nextSteps);

    const startBtn = $("shareStartBtn");
    if (startBtn) {
      startBtn.disabled = isHostSharing(payload);
    }
    const stopBtn = $("shareStopBtn");
    if (stopBtn) {
      stopBtn.disabled = !payload.serverRunning;
    }
  }

  function buildShareStatusSimple(payload) {
    const captureLabel = captureLabelValue(payload);
    const bridgeBusy = hasPendingHostBridgeCommand();

    if (isCaptureSelecting(payload)) {
      return {
        title: localMessage("waiting_for_you"),
        detail: guidedState.hostWindowHint || localMessage("share_picker_open_detail"),
        badge: localMessage("choosing"),
        tone: "warn",
      };
    }

    if (bridgeBusy) {
      return {
        title: localMessage("preparing_to_share"),
        detail: guidedState.hostWindowHint || localMessage("connecting_share_page"),
        badge: localMessage("processing"),
        tone: "warn",
      };
    }

    if (isHostSharing(payload) && Number(payload.viewers || 0) > 0) {
      return {
        title: localMessage("viewer_connected"),
        detail: captureLabel
          ? "正在共享“" + captureLabel + "”，对方设备已经接入。你可以继续共享，或结束本次共享。"
          : localMessage("sharing_started"),
        badge: localMessage("connected_plain"),
        tone: "ok",
      };
    }

    if (isHostSharing(payload)) {
      return {
        title: localMessage("sharing_in_progress"),
        detail: captureLabel
          ? "正在共享“" + captureLabel + "”，等待对方连接。你可以让对方扫码或打开地址进入。"
          : localMessage("sharing_waiting_viewer"),
        badge: localMessage("sharing_plain"),
        tone: "ok",
      };
    }

    if (guidedState.justStopped && !payload.serverRunning) {
      return {
        title: localMessage("sharing_stopped"),
        detail: localMessage("sharing_stopped_detail"),
        badge: localMessage("stopped_plain"),
        tone: "warn",
      };
    }

    if (baseServiceReady(payload)) {
      return {
        title: localMessage("waiting_content_selection"),
        detail: captureLabel
          ? "当前已选择“" + captureLabel + "”。你可以直接开始共享，或重新选择共享窗口。"
          : (guidedState.hostWindowHint || localMessage("share_picker_or_window_system_detail")),
        badge: localMessage("ready_to_start"),
        tone: "idle",
      };
    }

    if (payload.serverRunning) {
      return {
        title: localMessage("share_one_more_step"),
        detail: payload.dashboardError || localMessage("share_service_missing_one_step_started"),
        badge: localMessage("repair_needed"),
        tone: "warn",
      };
    }

    return {
      title: localMessage("sharing_not_ready"),
      detail: localMessage("return_to_guide_then_prepare_page"),
      badge: localMessage("not_ready"),
      tone: "idle",
    };
  }

  function renderShareSimple(payload) {
    const status = buildShareStatusSimple(payload);
    const attention = buildShareAttention(payload);
    const captureLabel = captureLabelValue(payload);
    const selecting = isCaptureSelecting(payload);
    const bridgeBusy = hasPendingHostBridgeCommand();

    setText("shareTitle", status.title);
    setText("shareDetail", status.detail);
    setStatusBadge("shareBadge", status.badge, status.tone);
    setText(
      "shareHelperText",
      selecting
        ? localMessage("share_picker_system_detail")
        : bridgeBusy
        ? (guidedState.hostWindowHint || localMessage("connecting_share_page"))
        : isHostSharing(payload)
        ? (captureLabel
          ? "当前正在共享“" + captureLabel + "”。你可以继续共享，也可以更换共享窗口。"
          : localMessage("already_started_change_window"))
        : (guidedState.hostWindowHint ||
          (payload.hotspotRunning
            ? localMessage("viewer_join_hotspot_system_dialog")
            : localMessage("hand_address_then_share")))
    );

    const shareAttentionCard = $("shareAttentionCard");
    if (shareAttentionCard) {
      shareAttentionCard.hidden = !attention;
    }
    setText("shareAttentionTitle", attention ? attention.title : localMessage("share_one_more_step"));
    setText("shareAttentionText", attention ? attention.text : localMessage("no_extra_reminder"));

    const shareAttentionBtn = $("shareAttentionBtn");
    if (shareAttentionBtn) {
      shareAttentionBtn.dataset.guidedAction = attention ? attention.action : "request-snapshot";
      shareAttentionBtn.textContent = attention ? attention.actionLabel : localMessage("refresh_status");
    }

    setPairs("shareAccessCard", [
      [localMessage("access_link"), payload.viewerUrl || localMessage("generated_after_prepare_paren")],
      [localMessage("qr_code"), payload.viewerUrl ? localMessage("click_show_qr") : localMessage("generated_after_prepare")],
      [localMessage("hotspot_name"), payload.hotspotRunning ? (payload.hotspotSsid || localMessage("unnamed")) : localMessage("not_used_plain")],
      [localMessage("hotspot_password"), payload.hotspotRunning ? (payload.hotspotPassword || localMessage("not_generated")) : localMessage("not_used_plain")]
    ]);

    setPairs("shareMethodCard", [
      [localMessage("current_path"), detectConnectionPath(payload)],
      [localMessage("sharing_address"), payload.hostIp || localMessage("waiting_for_detection")],
      [localMessage("hotspot_status"), payload.hotspotStatus || "stopped"],
      [localMessage("local_health"), yesNo(payload.healthReady, localMessage("normal_status"), localMessage("caution_needed"))]
    ]);

    setPairs("shareSessionCard", [
      [localMessage("current_status"), selecting ? localMessage("waiting_content_selection") : (bridgeBusy ? localMessage("preparing_to_share") : (isHostSharing(payload) ? localMessage("sharing_in_progress") : (payload.serverRunning ? localMessage("waiting_for_content") : localMessage("not_running_plain"))))],
      [localMessage("shared_target"), captureLabel || (selecting ? localMessage("waiting_for_selection") : localMessage("not_selected_yet"))],
      [localMessage("viewers_connected"), payload.viewers],
      [localMessage("handoff_status"), payload.handoffLabel || localMessage("not_started_plain")],
      [localMessage("latest_note"), payload.handoffDetail || localMessage("next_step_after_prepare")]
    ]);

    const nextSteps = [];
    if (selecting) {
      nextSteps.push([localMessage("choose_share_content"), localMessage("share_picker_system_choose")]);
    } else if (bridgeBusy) {
      nextSteps.push([localMessage("preparing_to_share"), localMessage("standalone_window_fallback")]);
    } else if (!isHostSharing(payload)) {
      nextSteps.push([localMessage("start_sharing_content"), localMessage("choose_window_system_dialog")]);
    } else if (Number(payload.viewers || 0) === 0) {
      nextSteps.push([localMessage("let_viewer_join"), payload.hotspotRunning
        ? localMessage("viewer_join_current_hotspot")
        : localMessage("viewer_stay_same_network")]);
    } else {
      nextSteps.push([localMessage("keep_sharing"), localMessage("viewer_connected_keep_sharing")]);
    }

    if (attention) {
      nextSteps.push([attention.title, attention.text]);
    } else if (payload.hotspotRunning) {
      nextSteps.push([localMessage("hotspot_info"), localMessage("hotspot_already_started_detail")]);
    } else {
      nextSteps.push([localMessage("access_method"), localMessage("prefer_link_or_qr")]);
    }
    renderSuggestions("shareNextSteps", nextSteps);

    const startBtn = $("shareStartBtn");
    if (startBtn) {
      startBtn.disabled = bridgeBusy || isHostSharing(payload) || selecting;
      startBtn.textContent = selecting ? localMessage("selecting_ellipsis") : (bridgeBusy ? localMessage("preparing_ellipsis") : (isHostSharing(payload) ? localMessage("already_sharing") : localMessage("start_sharing_button")));
    }
    const stopBtn = $("shareStopBtn");
    if (stopBtn) {
      stopBtn.disabled = (bridgeBusy && !selecting) || (!payload.serverRunning && !isHostSharing(payload) && !selecting);
    }
    const chooseBtn = $("shareChooseBtn");
    if (chooseBtn) {
      chooseBtn.disabled = bridgeBusy || selecting;
      chooseBtn.textContent = bridgeBusy ? localMessage("preparing_ellipsis") : (isHostSharing(payload) ? localMessage("change_shared_window") : localMessage("choose_window_button"));
    }
  }

  function renderShareDebug(payload) {
    setPairs("shareDebugMeta", [
      ["route", currentRoute],
      ["previewLoaded", previewLoaded],
      ["hostBridge.ready", hostBridge.ready],
      ["pendingCommand", hostBridge.pendingCommand ? (hostBridge.pendingCommand.command + " / " + hostBridge.pendingCommand.requestId) : "-"],
      ["inFlightRequestId", hostBridge.inFlightRequestId || "-"],
      ["hostUrl", payload.hostUrl || "-"],
      ["hostState", payload.hostState || "-"],
      ["captureState", payload.captureState || "-"],
      ["captureLabel", payload.captureLabel || "-"]
    ]);
    setText("shareDebugLog", debugState.entries.length ? debugState.entries.join("\n") : "No debug entries yet.");
  }

  function renderSimpleMode(payload) {
    renderGuide(payload);
    renderPrepare(payload);
    renderShareSimple(payload);
    renderShareDebug(payload);
  }

  function applySession() {
    sessionDirty = false;
    send({
      kind: "command",
      command: "apply-session",
      room: $("roomInput") ? $("roomInput").value || "" : "",
      token: $("tokenInput") ? $("tokenInput").value || "" : "",
      bind: $("bindInput") ? $("bindInput").value || "" : "",
      port: Number(($("portInput") ? $("portInput").value : "") || 9443)
    });
  }

  function applyHotspot() {
    hotspotDirty = false;
    send({
      kind: "command",
      command: "apply-hotspot",
      ssid: $("hotspotSsidInput") ? $("hotspotSsidInput").value || "" : "",
      password: $("hotspotPasswordInput") ? $("hotspotPasswordInput").value || "" : ""
    });
  }

  function syncHotspotActions(payload) {
    const startBtn = $("startHotspotBtn");
    const stopBtn = $("stopHotspotBtn");
    const applyBtn = $("applyHotspotBtn");
    if (!startBtn || !stopBtn || !applyBtn) return;

    const directControl = !!payload.hotspotSupported;
    applyBtn.disabled = false;
    stopBtn.disabled = !payload.hotspotRunning;

    if (directControl) {
      startBtn.disabled = !!payload.hotspotRunning;
      startBtn.textContent = "Start Hotspot";
      startBtn.title = "";
    } else {
      startBtn.disabled = false;
      startBtn.textContent = "Use System Hotspot";
      startBtn.title = "This machine does not support hostednetwork control. Open Windows hotspot settings instead.";
    }
  }

  function handleCommand(command, extra) {
    debugLog("handleCommand", { command, extra: extra || null });
    if (command === "request-snapshot") {
      send({ kind: "request-snapshot" });
      return;
    }
    if (command === "apply-session") {
      applySession();
      return;
    }
    if (command === "apply-hotspot") {
      applyHotspot();
      return;
    }
    if (command === "start-hotspot") {
      if (hotspotDirty) {
        applyHotspot();
      }
      if (!state.hotspotSupported && !state.hotspotRunning) {
        send({ kind: "command", command: "open-hotspot-settings" });
        return;
      }
    }
    send({ kind: "command", command, ...extra });
  }

  function openGuidedShareWindow() {
    guidedState.justStopped = false;
    guidedState.pendingHostOpen = true;
    if (isHostSharing(state)) {
      guidedState.hostWindowHint = localMessage("share_window_open_change_later");
      handleCommand("open-host");
    } else if (!state.serverRunning) {
      guidedState.hostWindowHint = localMessage("share_window_will_open_first");
      handleCommand("start-and-open-host");
    } else {
      guidedState.hostWindowHint = localMessage("share_window_open_click_start");
      handleCommand("open-host");
    }
    switchRoute("share");
    requestRender();
  }

  function runGuidedAction(action) {
    debugLog("runGuidedAction", {
      action,
      route: currentRoute,
      disabledStart: $("shareStartBtn") ? $("shareStartBtn").disabled : null,
      disabledChoose: $("shareChooseBtn") ? $("shareChooseBtn").disabled : null,
      disabledStop: $("shareStopBtn") ? $("shareStopBtn").disabled : null,
    });
    switch (action) {
      case "resume-share":
        switchRoute("share");
        requestRender();
        return;
      case "go-guide":
        switchRoute("guide");
        requestRender();
        return;
      case "switch-hotspot":
        guidedState.activeMode = "hotspot";
        guidedState.notice = localMessage("switching_to_hotspot_for_connection");
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.hotspotAuto = false;
        guidedState.actions.hotspotStarted = false;
        guidedState.lastCommandAt = 0;
        requestRender();
        return;
      case "retry-hotspot":
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.hotspotStarted = false;
        guidedState.lastCommandAt = 0;
        requestRender();
        return;
      case "retry-service":
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.serverStarted = false;
        guidedState.lastCommandAt = 0;
        handleCommand("stop-server");
        requestRender();
        return;
      case "open-share":
      case "choose-share":
        startGuidedShareCommand(action === "choose-share" ? "choose-share" : "start-share");
        return;
      case "stop-share":
        stopGuidedShareCommand();
        return;
        guidedState.pendingHostOpen = false;
        guidedState.justStopped = true;
        guidedState.hostWindowHint = localMessage("share_stopped_back_guide");
        handleCommand("stop-server");
        requestRender();
        return;
      default:
        break;
    }

    handleCommand(action);
  }

  function handleLocalAction(action) {
    if (action === "clear-share-debug") {
      debugState.entries = [];
      debugLog("debug log cleared");
      requestRender();
      return;
    }
    if (action === "reload-preview") {
      const frame = $("hostPreviewFrame");
      if (!frame) return;
      if (!state.serverRunning || !state.hostUrl) {
        handleCommand("request-snapshot");
        return;
      }
      previewLoaded = false;
      hostBridge.ready = false;
      previewUrl = state.hostUrl;
      frame.src = state.hostUrl;
      updateHostPreview(state);
    }
  }

  function resolveButtonFromEventTarget(target) {
    if (target && typeof target.closest === "function") {
      return target.closest("button");
    }
    if (target && target.parentElement && typeof target.parentElement.closest === "function") {
      return target.parentElement.closest("button");
    }
    return null;
  }

  function bindButtons() {
    document.addEventListener("click", (event) => {
      const button = resolveButtonFromEventTarget(event.target);
      if (!button) return;

      if (button.hasAttribute("data-route-target")) {
        switchRoute(button.getAttribute("data-route-target"));
        requestRender();
        return;
      }

      if (button.hasAttribute("data-guided-choice")) {
        startGuidedFlow(button.getAttribute("data-guided-choice"));
        return;
      }

      if (button.hasAttribute("data-guided-action")) {
        runGuidedAction(button.getAttribute("data-guided-action"));
        return;
      }

      if (button.hasAttribute("data-tab")) {
        switchTab(button.getAttribute("data-tab"), true);
        return;
      }
      if (button.hasAttribute("data-tab-target")) {
        switchTab(button.getAttribute("data-tab-target"), true);
        return;
      }

      if (button.hasAttribute("data-local-action")) {
        handleLocalAction(button.getAttribute("data-local-action"));
        return;
      }

      const command = button.getAttribute("data-command");
      if (!command) return;
      const index = button.getAttribute("data-index");
      handleCommand(command, index === null ? {} : { index: Number(index) });
    });

    const shareStartBtn = $("shareStartBtn");
    if (shareStartBtn) {
      shareStartBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareStartBtn click", { disabled: shareStartBtn.disabled });
        if (!shareStartBtn.disabled) {
          runGuidedAction("open-share");
        }
      });
    }

    const shareStopBtn = $("shareStopBtn");
    if (shareStopBtn) {
      shareStopBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareStopBtn click", { disabled: shareStopBtn.disabled });
        if (!shareStopBtn.disabled) {
          runGuidedAction("stop-share");
        }
      });
    }

    const shareChooseBtn = $("shareChooseBtn");
    if (shareChooseBtn) {
      shareChooseBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareChooseBtn click", { disabled: shareChooseBtn.disabled });
        if (!shareChooseBtn.disabled) {
          runGuidedAction("choose-share");
        }
      });
    }

    ["roomInput", "tokenInput", "bindInput", "portInput"].forEach((id) => {
      const node = $(id);
      if (!node) return;
      node.addEventListener("input", () => {
        sessionDirty = true;
      });
    });

    ["hotspotSsidInput", "hotspotPasswordInput"].forEach((id) => {
      const node = $(id);
      if (!node) return;
      node.addEventListener("input", () => {
        hotspotDirty = true;
      });
    });

    const previewFrame = $("hostPreviewFrame");
    if (previewFrame) {
      previewFrame.addEventListener("load", () => {
        debugLog("hostPreviewFrame load", { src: previewFrame.src || "" });
        previewLoaded = true;
        updateHostPreview(state);
        tryFlushHostBridgeCommand();
      });
    }

    document.querySelectorAll("[data-language-select]").forEach((select) => {
      if (select.dataset.boundLanguage === "true") return;
      select.dataset.boundLanguage = "true";
      select.addEventListener("change", () => {
        const locale = select.value || "en";
        if (window.LanShareI18n && typeof window.LanShareI18n.setLocale === "function") {
          pendingLocaleOverride = window.LanShareI18n.setLocale(locale);
        }
        applyLocalTranslations(document.body || document.documentElement);
        requestRender();
        if (window.chrome && window.chrome.webview) {
          send({ kind: "command", command: "set-language", locale });
        }
      });
    });
  }

  function bindBridge() {
    if (!window.chrome || !window.chrome.webview) {
      setText("bridgeStatus", "Running outside WebView2");
      applyLocalTranslations(document.body || document.documentElement);
      return;
    }

    window.chrome.webview.addEventListener("message", (event) => {
      const message = event.data || {};
      if (message.name === "state.snapshot") {
        debugLog("recv state.snapshot", {
          serverRunning: !!(message.payload && message.payload.serverRunning),
          hostUrl: message.payload && message.payload.hostUrl ? message.payload.hostUrl : "",
          hostState: message.payload && message.payload.hostState ? message.payload.hostState : "",
          captureState: message.payload && message.payload.captureState ? message.payload.captureState : "",
        });
        render(message.payload || {});
      }
    });

    window.addEventListener("message", (event) => {
      const message = event.data || {};
      if (message.source !== "lan-share-host") {
        return;
      }
      debugLog("recv host message", {
        kind: message.kind || "",
        requestId: message.requestId || "",
        state: message.state || "",
        captureState: message.captureState || "",
        captureLabel: message.captureLabel || "",
      });

      if (message.kind === "bridge-ready") {
        hostBridge.ready = true;
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });
        tryFlushHostBridgeCommand();
        return;
      }

      if (message.kind === "status") {
        hostBridge.ready = true;
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          viewers: Object.prototype.hasOwnProperty.call(message, "viewers") ? message.viewers : state.viewers,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });
        tryFlushHostBridgeCommand();
        return;
      }

      if (message.kind === "command-result") {
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          viewers: Object.prototype.hasOwnProperty.call(message, "viewers") ? message.viewers : state.viewers,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });

        if (!hostBridgeRequestMatches(message.requestId)) {
          return;
        }

        const pending = hostBridge.pendingCommand;
        finalizeHostBridgeRequest(message.requestId);
        guidedState.pendingHostOpen = false;
        const detailText = String(message.detail || "").toLowerCase();
        const selectionCanceled =
          detailText.includes("cancel") ||
          detailText.includes("denied") ||
          detailText.includes("dismiss") ||
          detailText.includes("abort");

        if (message.ok) {
          if (message.command === "stop-share" && pending && pending.stopServerAfter) {
            guidedState.hostWindowHint = localMessage("finishing_share");
            handleCommand("stop-server");
          } else if (message.command === "choose-share") {
            guidedState.hostWindowHint = localMessage("share_target_updated");
          } else if (message.command === "start-share") {
            guidedState.hostWindowHint = localMessage("share_started_waiting_viewer_short");
          }
        } else if (pending && pending.stopServerAfter) {
          guidedState.hostWindowHint = localMessage("share_stopped_short");
          handleCommand("stop-server");
        } else if (selectionCanceled) {
          guidedState.hostWindowHint = localMessage("share_selection_canceled");
        } else if (pending) {
          fallbackOpenHostWindow(pending.command);
          finalizeHostBridgeRequest(message.requestId);
          return;
        } else {
          guidedState.hostWindowHint = localMessage("share_start_failed");
        }

        requestRender();
      }
    });

    send({ kind: "ready" });
    emitDomProbe("bridge-ready");
  }

  function renderAdvancedShell(payload) {
    const nativeTab = normalizeTab(state.nativePage);
    if (nativeTab !== activeTab) {
      switchTab(nativeTab, false);
    }
    setText("bridgeStatus", state.serverRunning ? localMessage("bridge_live_running") : localMessage("bridge_live"));
    const bridgeNode = $("bridgeStatus");
    if (bridgeNode) {
      bridgeNode.className = "status " + (state.serverRunning ? "ok" : "idle");
    }

    const detailLine = state.dashboardState === "ready"
      ? localMessage("dashboard_ready_detail")
      : state.dashboardState === "sharing"
      ? localMessage("dashboard_sharing_detail")
      : state.dashboardState === "error"
      ? localMessage("dashboard_error_detail")
      : localMessage("dashboard_not_ready_detail");

    setText("dashboardStateLabel", state.dashboardLabel || "Unknown");
    const dashboardLabel = $("dashboardStateLabel");
    if (dashboardLabel) {
      dashboardLabel.className = "status-label tone-" + (state.dashboardState || "not-ready");
    }
    setText("dashboardStateDetail", detailLine);

    setPairs("dashboardStatusGrid", [
      ["Host IP", state.hostIp],
      ["Port", state.port],
      ["Room", state.room],
      ["Viewers", state.viewers],
      ["Latest Error", state.dashboardError]
    ]);

    setPairs("dashboardNetworkCard", [
      ["Primary IPv4", state.hostIp],
      ["Network Mode", state.networkMode],
      ["Adapters", state.activeIpv4Candidates],
      ["Hotspot", state.hotspotStatus]
    ]);

    setPairs("dashboardServiceCard", [
      ["Server EXE", state.serverExePath],
      ["Bind + Port", (state.bind || "-") + ":" + (state.port || "-")],
      ["Admin UI Dir", state.adminDir],
      ["Host State", state.hostState]
    ]);

    setPairs("dashboardSharingCard", [
      ["Viewer URL", state.viewerUrl],
      ["Capture State", state.captureState],
      ["Capture Target", state.captureLabel],
      ["Copied", state.viewerUrlCopied],
      ["Bundle Exported", state.shareBundleExported],
      ["Bundle Dir", state.bundleDir]
    ]);

    setPairs("dashboardHealthCard", [
      ["/health", state.healthReady],
      ["Reachability", state.hostReachable],
      ["Heartbeat", state.recentHeartbeat],
      ["WebView", state.webviewStatus]
    ]);

    setPairs("dashboardHandoffCard", handoffSummary(state));
    renderQuickFixes(state);
    renderSuggestions("suggestions", dashboardSuggestions(state));

    applySessionForm(state);
    applyHotspotForm(state);
    renderCandidates(state);
    syncHotspotActions(state);

    setPairs("sessionSummary", [
      ["Host URL", state.hostUrl],
      ["Viewer URL", state.viewerUrl],
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir]
    ]);

    setPairs("sessionRuntimeCard", [
      ["Server Running", state.serverRunning],
      ["Rooms", state.rooms],
      ["Viewers", state.viewers],
      ["Recent Heartbeat", state.recentHeartbeat],
      ["Host Ready State", state.hostState],
      ["Capture State", state.captureState],
      ["Capture Target", state.captureLabel],
      ["Local Reachability", state.localReachability]
    ]);
    updateHostPreview(state);

    setPairs("networkSummary", [
      ["Recommended IPv4", state.hostIp],
      ["Current Bind", state.bind],
      ["Reachability", state.localReachability],
      ["Firewall Path", state.firewallReady ? "Ready" : "Needs attention"],
      ["Remote Viewer Path", state.remoteViewerReady ? "Ready" : "Needs attention"],
      ["Wi-Fi Adapter", state.wifiAdapterPresent],
      ["Hotspot Supported", state.hotspotSupported],
      ["Current Hotspot State", state.hotspotStatus]
    ]);

    setPairs("networkDiagnosticsCard", [
      ["Firewall Path", state.firewallReady ? "Ready" : "Needs attention"],
      ["Firewall Detail", state.firewallDetail],
      ["Remote Viewer Path", state.remoteViewerReady ? "Ready" : "Needs attention"],
      ["Remote Viewer Detail", state.remoteViewerDetail],
      ["Probe Summary", state.remoteProbeLabel],
      ["Next Action", state.remoteProbeAction]
    ]);

    setPairs("wifiDirectCard", [
      ["Wi-Fi Direct", state.wifiDirectAvailable],
      ["Recommendation", state.wifiDirectAvailable ? "Use Connected Devices pairing in Windows." : "Use LAN or hotspot path."],
      ["Current Session Alias", state.room ? "ViewMesh-" + state.room : "ViewMesh-session"]
    ]);

    setPairs("hotspotCard", [
      ["Status", state.hotspotStatus],
      ["SSID", state.hotspotSsid],
      ["Password", state.hotspotPassword],
      ["Network Mode", state.networkMode]
    ]);

    setPairs("sharingAccessCard", [
      ["Host URL", state.hostUrl],
      ["Viewer URL", state.viewerUrl],
      ["Viewer Copied", state.viewerUrlCopied],
      ["Bundle Exported", state.shareBundleExported]
    ]);

    setPairs("sharingMaterialCard", [
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir],
      ["Server EXE", state.serverExePath],
      ["Current Warning", state.dashboardError]
    ]);

    setHtml("sharingGuide", sharingGuide(state).map(([label, detail]) => {
      return "<article class=\"guide\"><h3>" + label + "</h3><p>" + detail + "</p></article>";
    }).join(""));

    renderMetrics(state);
    setText("timelineText", state.timelineText || "No timeline events yet.");
    setText("logTailText", state.logTail || "No logs yet.");

    renderChecklist(state);
    const diagActions = [
      ["Check subnet alignment", "Confirm the viewer device and the host still sit on the same local network."]
    ];
    if (state.serverRunning && !state.firewallReady) {
      diagActions.push(["Open Windows Firewall settings", state.firewallDetail || "Confirm an inbound allow rule exists for the current share executable or TCP port."]);
    }
    if (state.serverRunning && !state.remoteViewerReady) {
      diagActions.push(["Collect a local network diagnostics report", state.remoteViewerDetail || "Run the helper report before retrying from another device."]);
      diagActions.push(["Export a remote-device probe guide", state.remoteProbeAction || "Generate a checklist with candidate LAN URLs and remote browser test steps."]);
    }
    diagActions.push(["Test URL directly", "If Viewer fails, paste the Viewer URL directly into a browser first."]);
    diagActions.push(["Fallback to system hotspot", "If hotspot start fails, open the Windows hotspot settings and start it manually."]);
    renderSuggestions("diagActions", diagActions);
    setPairs("diagPaths", [
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir],
      ["Admin UI Dir", state.adminDir],
      ["Server EXE", state.serverExePath]
    ]);
    setText("diagWarningText", state.dashboardError || "None");

    setPairs("settingsGeneral", [
      ["Default Port", state.defaultPort],
      ["Default Bind", state.defaultBind],
      ["Room Rule", state.roomRule],
      ["Token Rule", state.tokenRule]
    ]);
    setPairs("settingsSharing", [
      ["Viewer Open Mode", state.defaultViewerOpenMode],
      ["Auto Copy Viewer Link", state.autoCopyViewerLink],
      ["Auto Generate QR", state.autoGenerateQr],
      ["Auto Export Bundle", state.autoExportBundle]
    ]);
    setPairs("settingsLogging", [
      ["Log Level", state.logLevel],
      ["Save stdout/stderr", state.saveStdStreams],
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir]
    ]);
    setPairs("settingsAdvanced", [
      ["WebView Behavior", state.webViewBehavior],
      ["Startup Hook", state.startupHook],
      ["Current Native Page", state.nativePage]
    ]);

    setText("rawState", JSON.stringify(payload || {}, null, 2));
  }

  function render(payload) {
    Object.assign(state, payload || {});
    if (window.LanShareI18n && typeof window.LanShareI18n.setLocale === "function") {
      const snapshotLocale = state.locale ? String(state.locale) : "";
      if (pendingLocaleOverride && snapshotLocale === pendingLocaleOverride) {
        pendingLocaleOverride = "";
      }
      const activeLocale = pendingLocaleOverride || snapshotLocale;
      if (activeLocale) {
        window.LanShareI18n.setLocale(activeLocale, { persist: false, apply: false });
      }
    }
    updateRouteFromSnapshot(state);
    syncGuidedShareFlags(state);
    renderSimpleMode(state);
    renderAdvancedShell(state);
    runGuidedPreparation(state);
    tryFlushHostBridgeCommand();
    if (window.LanShareI18n && typeof window.LanShareI18n.applyDocument === "function") {
      window.LanShareI18n.applyDocument();
    }
    applyLocalTranslations(document.body || document.documentElement);
    emitDomProbe("render");
  }

  bindButtons();
  switchRoute(currentRoute);
  switchTab(activeTab, false);
  applyLocalTranslations(document.body || document.documentElement);
  debugLog("simple shell initialized", { route: currentRoute });
  bindBridge();
})();
