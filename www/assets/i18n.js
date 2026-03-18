(function () {
  const STORAGE_KEY = "lan_share_locale";
  const SUPPORTED = [
    { code: "en", label: "English" },
    { code: "zh-CN", label: "简体中文" },
    { code: "zh-TW", label: "繁體中文" },
    { code: "ko", label: "한국어" },
    { code: "ja", label: "日本語" },
    { code: "de", label: "Deutsch" },
    { code: "ru", label: "Русский" },
  ];

  const CATALOG = {
    language: { en: "Language", "zh-CN": "语言", "zh-TW": "語言", ko: "언어", ja: "言語", de: "Sprache", ru: "Язык" },
    dashboard: { en: "Dashboard", "zh-CN": "总览", "zh-TW": "總覽", ko: "대시보드", ja: "ダッシュボード", de: "Dashboard", ru: "Панель" },
    session: { en: "Session", "zh-CN": "会话", "zh-TW": "工作階段", ko: "세션", ja: "セッション", de: "Sitzung", ru: "Сессия" },
    network: { en: "Network", "zh-CN": "网络", "zh-TW": "網路", ko: "네트워크", ja: "ネットワーク", de: "Netzwerk", ru: "Сеть" },
    sharing: { en: "Sharing", "zh-CN": "共享", "zh-TW": "分享", ko: "공유", ja: "共有", de: "Freigabe", ru: "Показ" },
    monitor: { en: "Monitor", "zh-CN": "监控", "zh-TW": "監控", ko: "모니터", ja: "モニター", de: "Monitor", ru: "Монитор" },
    diagnostics: { en: "Diagnostics", "zh-CN": "诊断", "zh-TW": "診斷", ko: "진단", ja: "診断", de: "Diagnose", ru: "Диагностика" },
    settings: { en: "Settings", "zh-CN": "设置", "zh-TW": "設定", ko: "설정", ja: "設定", de: "Einstellungen", ru: "Настройки" },
    host: { en: "Host", "zh-CN": "主控端", "zh-TW": "主控端", ko: "호스트", ja: "ホスト", de: "Host", ru: "Хост" },
    viewer: { en: "Viewer", "zh-CN": "接收端", "zh-TW": "接收端", ko: "뷰어", ja: "ビューアー", de: "Viewer", ru: "Клиент" },
    lan_host: { en: "LAN Host", "zh-CN": "局域网主控端", "zh-TW": "區域網主控端", ko: "LAN 호스트", ja: "LAN ホスト", de: "LAN-Host", ru: "LAN-хост" },
    lan_viewer: { en: "LAN Viewer", "zh-CN": "局域网接收端", "zh-TW": "區域網接收端", ko: "LAN 뷰어", ja: "LAN ビューアー", de: "LAN-Viewer", ru: "LAN Viewer" },
    room: { en: "Room", "zh-CN": "房间", "zh-TW": "房間", ko: "룸", ja: "ルーム", de: "Raum", ru: "Комната" },
    viewers: { en: "Viewers", "zh-CN": "接收方", "zh-TW": "接收方", ko: "뷰어", ja: "閲覧側", de: "Viewer", ru: "Зрители" },
    token: { en: "Token", "zh-CN": "令牌", "zh-TW": "權杖", ko: "토큰", ja: "トークン", de: "Token", ru: "Токен" },
    signal: { en: "Signal", "zh-CN": "信令", "zh-TW": "訊號", ko: "신호", ja: "シグナル", de: "Signal", ru: "Сигнал" },
    capture: { en: "Capture", "zh-CN": "采集", "zh-TW": "擷取", ko: "캡처", ja: "キャプチャ", de: "Erfassung", ru: "Захват" },
    idle: { en: "idle", "zh-CN": "空闲", "zh-TW": "閒置", ko: "유휴", ja: "待機", de: "bereit", ru: "ожидание" },
    waiting: { en: "Waiting", "zh-CN": "等待中", "zh-TW": "等待中", ko: "대기 중", ja: "待機中", de: "Warten", ru: "Ожидание" },
    connecting: { en: "connecting", "zh-CN": "连接中", "zh-TW": "連線中", ko: "연결 중", ja: "接続中", de: "verbinde", ru: "подключение" },
    not_started: { en: "not started", "zh-CN": "未开始", "zh-TW": "未開始", ko: "시작 안 함", ja: "未開始", de: "nicht gestartet", ru: "не начато" },
    browser: { en: "browser", "zh-CN": "浏览器", "zh-TW": "瀏覽器", ko: "브라우저", ja: "ブラウザー", de: "Browser", ru: "браузер" },
    fullscreen: { en: "fullscreen", "zh-CN": "全屏", "zh-TW": "全螢幕", ko: "전체 화면", ja: "全画面", de: "Vollbild", ru: "полный экран" },
    installed: { en: "installed", "zh-CN": "已安装", "zh-TW": "已安裝", ko: "설치됨", ja: "インストール済み", de: "installiert", ru: "установлено" },
    missing: { en: "(missing)", "zh-CN": "（缺失）", "zh-TW": "（缺失）", ko: "(없음)", ja: "（未設定）", de: "(fehlt)", ru: "(отсутствует)" },
    language_title: { en: "LanScreenShare Guide & Console", "zh-CN": "LanScreenShare 引导与控制台", "zh-TW": "LanScreenShare 引導與控制台", ko: "LanScreenShare 안내 및 콘솔", ja: "LanScreenShare ガイドとコンソール", de: "LanScreenShare Anleitung und Konsole", ru: "LanScreenShare: мастер и консоль" },
    re_detect: { en: "Re-Detect", "zh-CN": "重新检测", "zh-TW": "重新偵測", ko: "다시 감지", ja: "再検出", de: "Erneut erkennen", ru: "Проверить снова" },
    advanced_diagnostics: { en: "Advanced Diagnostics", "zh-CN": "高级诊断", "zh-TW": "進階診斷", ko: "고급 진단", ja: "詳細診断", de: "Erweiterte Diagnose", ru: "Расширенная диагностика" },
    start_sharing: { en: "Start Sharing", "zh-CN": "开始共享", "zh-TW": "開始分享", ko: "공유 시작", ja: "共有を開始", de: "Freigabe starten", ru: "Начать показ" },
    stop_sharing: { en: "Stop Sharing", "zh-CN": "停止共享", "zh-TW": "停止分享", ko: "공유 중지", ja: "共有を停止", de: "Freigabe stoppen", ru: "Остановить показ" },
    install_app: { en: "Install App", "zh-CN": "安装应用", "zh-TW": "安裝 App", ko: "앱 설치", ja: "アプリをインストール", de: "App installieren", ru: "Установить приложение" },
    enter_fullscreen: { en: "Enter Fullscreen", "zh-CN": "进入全屏", "zh-TW": "進入全螢幕", ko: "전체 화면", ja: "全画面にする", de: "Vollbild", ru: "Во весь экран" },
    exit_fullscreen: { en: "Exit Fullscreen", "zh-CN": "退出全屏", "zh-TW": "離開全螢幕", ko: "전체 화면 종료", ja: "全画面を終了", de: "Vollbild beenden", ru: "Выйти из полного экрана" },
    hide_hud: { en: "Hide HUD", "zh-CN": "隐藏 HUD", "zh-TW": "隱藏 HUD", ko: "HUD 숨기기", ja: "HUD を隠す", de: "HUD ausblenden", ru: "Скрыть HUD" },
    show_hud: { en: "Show HUD", "zh-CN": "显示 HUD", "zh-TW": "顯示 HUD", ko: "HUD 표시", ja: "HUD を表示", de: "HUD anzeigen", ru: "Показать HUD" },
    debug: { en: "Debug", "zh-CN": "调试", "zh-TW": "偵錯", ko: "디버그", ja: "デバッグ", de: "Debug", ru: "Отладка" },
    debug_log: { en: "Debug Log", "zh-CN": "调试日志", "zh-TW": "偵錯日誌", ko: "디버그 로그", ja: "デバッグログ", de: "Debug-Protokoll", ru: "Журнал отладки" },
    close: { en: "Close", "zh-CN": "关闭", "zh-TW": "關閉", ko: "닫기", ja: "閉じる", de: "Schließen", ru: "Закрыть" },
    clear: { en: "Clear", "zh-CN": "清空", "zh-TW": "清空", ko: "지우기", ja: "クリア", de: "Leeren", ru: "Очистить" },
    no_debug_entries: { en: "No debug entries yet.", "zh-CN": "暂无调试记录。", "zh-TW": "尚無偵錯紀錄。", ko: "아직 디버그 항목이 없습니다.", ja: "まだデバッグ記録はありません。", de: "Noch keine Debug-Einträge.", ru: "Пока нет записей отладки." },
    none: { en: "None", "zh-CN": "无", "zh-TW": "無", ko: "없음", ja: "なし", de: "Keine", ru: "Нет" },
    no_logs_yet: { en: "No logs yet.", "zh-CN": "暂无日志。", "zh-TW": "尚無日誌。", ko: "아직 로그가 없습니다.", ja: "まだログはありません。", de: "Noch keine Logs.", ru: "Логов пока нет." },
  };

  const REVERSE = new Map();
  Object.keys(CATALOG).forEach((key) => {
    Object.keys(CATALOG[key]).forEach((locale) => {
      REVERSE.set(CATALOG[key][locale], key);
    });
  });

  function normalize(locale) {
    const value = String(locale || "").trim().replace(/_/g, "-").toLowerCase();
    if (!value) return "en";
    if (value === "en" || value.startsWith("en-")) return "en";
    if (value === "zh" || value === "zh-cn" || value === "zh-sg" || value === "zh-hans" || value.startsWith("zh-cn-") || value.startsWith("zh-hans-")) return "zh-CN";
    if (value === "zh-tw" || value === "zh-hk" || value === "zh-mo" || value === "zh-hant" || value.startsWith("zh-tw-") || value.startsWith("zh-hant-")) return "zh-TW";
    if (value === "ko" || value.startsWith("ko-")) return "ko";
    if (value === "ja" || value.startsWith("ja-")) return "ja";
    if (value === "de" || value.startsWith("de-")) return "de";
    if (value === "ru" || value.startsWith("ru-")) return "ru";
    return "en";
  }

  function currentUrlLocale() {
    try {
      return new URL(location.href).searchParams.get("lang") || "";
    } catch {
      return "";
    }
  }

  function detectLocale() {
    const rawUrl = currentUrlLocale();
    if (rawUrl) return normalize(rawUrl);
    try {
      const storedRaw = localStorage.getItem(STORAGE_KEY) || "";
      if (storedRaw) return normalize(storedRaw);
    } catch {}
    const languages = Array.isArray(navigator.languages) && navigator.languages.length ? navigator.languages : [navigator.language || document.documentElement.lang || "en"];
    return normalize(languages.find(Boolean) || "en");
  }

  Object.assign(CATALOG, {
    quick_start: { en: "Quick Start", "zh-CN": "Quick Start", "zh-TW": "Quick Start", ko: "빠른 시작", ja: "クイックスタート", de: "Schnellstart", ru: "Быстрый старт" },
    current_environment: { en: "Current Environment", "zh-CN": "当前环境", "zh-TW": "目前環境", ko: "현재 환경", ja: "現在の環境", de: "Aktuelle Umgebung", ru: "Текущая среда" },
    current_session: { en: "Current Session", "zh-CN": "当前会话", "zh-TW": "目前工作階段", ko: "현재 세션", ja: "現在のセッション", de: "Aktuelle Sitzung", ru: "Текущая сессия" },
    connection_status: { en: "Connection Status", "zh-CN": "连接状态", "zh-TW": "連線狀態", ko: "연결 상태", ja: "接続状態", de: "Verbindungsstatus", ru: "Состояние подключения" },
    next_step: { en: "Next Step", "zh-CN": "下一步", "zh-TW": "下一步", ko: "다음 단계", ja: "次の手順", de: "Nächster Schritt", ru: "Следующий шаг" },
    refresh_network: { en: "Refresh Network", "zh-CN": "刷新网络", "zh-TW": "重新整理網路", ko: "네트워크 새로 고침", ja: "ネットワークを更新", de: "Netzwerk aktualisieren", ru: "Обновить сеть" },
    refresh_snapshot: { en: "Refresh Snapshot", "zh-CN": "刷新快照", "zh-TW": "重新整理快照", ko: "스냅샷 새로 고침", ja: "スナップショットを更新", de: "Snapshot aktualisieren", ru: "Обновить снимок" },
    continue_setup: { en: "Continue Setup", "zh-CN": "继续设置", "zh-TW": "繼續設定", ko: "설정 계속", ja: "設定を続ける", de: "Einrichtung fortsetzen", ru: "Продолжить настройку" },
    waiting_bridge: { en: "Waiting for bridge", "zh-CN": "等待桥接", "zh-TW": "等待橋接", ko: "브리지 대기 중", ja: "ブリッジ待機中", de: "Warten auf Bridge", ru: "Ожидание моста" },
    return_simple_mode: { en: "Back to Simple Mode", "zh-CN": "返回简化模式", "zh-TW": "返回簡化模式", ko: "간단 모드로 돌아가기", ja: "簡易モードへ戻る", de: "Zurück zum einfachen Modus", ru: "Назад в простой режим" },
    current_readiness: { en: "Current Readiness", "zh-CN": "当前就绪状态", "zh-TW": "目前就緒狀態", ko: "현재 준비 상태", ja: "現在の準備状況", de: "Aktuelle Bereitschaft", ru: "Текущая готовность" },
    request_state_bridge: { en: "Requesting state from the native bridge.", "zh-CN": "正在从原生桥接请求状态。", "zh-TW": "正在從原生橋接要求狀態。", ko: "네이티브 브리지에서 상태를 요청하는 중입니다.", ja: "ネイティブブリッジから状態を取得しています。", de: "Status wird von der nativen Bridge angefordert.", ru: "Запрашивается состояние у нативного моста." },
    service: { en: "Service", "zh-CN": "服务", "zh-TW": "服務", ko: "서비스", ja: "サービス", de: "Dienst", ru: "Служба" },
    health: { en: "Health", "zh-CN": "健康", "zh-TW": "健康", ko: "상태", ja: "健全性", de: "Gesundheit", ru: "Состояние" },
    copy_address: { en: "Copy Link", "zh-CN": "复制地址", "zh-TW": "複製位址", ko: "주소 복사", ja: "リンクをコピー", de: "Link kopieren", ru: "Скопировать адрес" },
    show_qr: { en: "Show QR Code", "zh-CN": "显示二维码", "zh-TW": "顯示 QR Code", ko: "QR 표시", ja: "QR を表示", de: "QR-Code anzeigen", ru: "Показать QR-код" },
    choose_window: { en: "Choose Window", "zh-CN": "选择共享窗口", "zh-TW": "選擇分享視窗", ko: "공유 창 선택", ja: "共有ウィンドウを選択", de: "Fenster auswählen", ru: "Выбрать окно" },
    host_preview: { en: "Host Preview", "zh-CN": "Host 预览", "zh-TW": "Host 預覽", ko: "호스트 미리보기", ja: "ホストプレビュー", de: "Host-Vorschau", ru: "Предпросмотр хоста" },
    host_preview_unavailable: { en: "Host Preview Unavailable", "zh-CN": "Host 预览不可用", "zh-TW": "Host 預覽無法使用", ko: "호스트 미리보기를 사용할 수 없음", ja: "ホストプレビューは利用できません", de: "Host-Vorschau nicht verfügbar", ru: "Предпросмотр хоста недоступен" },
    reload_preview: { en: "Reload Preview", "zh-CN": "重新加载预览", "zh-TW": "重新載入預覽", ko: "미리보기 다시 불러오기", ja: "プレビューを再読み込み", de: "Vorschau neu laden", ru: "Перезагрузить предпросмотр" },
    open_host_in_browser: { en: "Open Host In Browser", "zh-CN": "在浏览器中打开 Host", "zh-TW": "在瀏覽器中開啟 Host", ko: "브라우저에서 호스트 열기", ja: "ブラウザーでホストを開く", de: "Host im Browser öffnen", ru: "Открыть хост в браузере" },
    start_service: { en: "Start Service", "zh-CN": "启动服务", "zh-TW": "啟動服務", ko: "서비스 시작", ja: "サービス開始", de: "Dienst starten", ru: "Запустить службу" },
    loading_outside_webview2: { en: "Running outside WebView2", "zh-CN": "当前在 WebView2 外运行", "zh-TW": "目前在 WebView2 外執行", ko: "현재 WebView2 외부에서 실행 중", ja: "現在 WebView2 の外部で実行中", de: "Derzeit außerhalb von WebView2 ausgeführt", ru: "Сейчас работает вне WebView2" },
    native_style_console: { en: "Native-style sharing console", "zh-CN": "原生风格共享控制台", "zh-TW": "原生風格分享控制台", ko: "네이티브 스타일 공유 콘솔", ja: "ネイティブ風の共有コンソール", de: "Freigabekonsole im nativen Stil", ru: "Консоль доступа в нативном стиле" },
    start_without_browser_shell: { en: "Start sharing without a browser-looking shell.", "zh-CN": "不用浏览器外壳风格也能开始共享。", "zh-TW": "不用瀏覽器外殼風格也能開始分享。", ko: "브라우저 같은 외형 없이도 공유를 시작할 수 있습니다.", ja: "ブラウザーらしい外観を出さずに共有を始められます。", de: "Starten Sie die Freigabe ohne browserartige Oberfläche.", ru: "Начинайте показ без браузерного внешнего вида." },
    open_host_with_params: { en: "Open this page with /host?room=<ROOM>&token=<TOKEN>, then start screen sharing.", "zh-CN": "使用 /host?room=<ROOM>&token=<TOKEN> 打开此页面，然后开始共享屏幕。", "zh-TW": "使用 /host?room=<ROOM>&token=<TOKEN> 開啟此頁面，然後開始分享螢幕。", ko: "/host?room=<ROOM>&token=<TOKEN> 으로 이 페이지를 연 뒤 화면 공유를 시작하세요.", ja: "/host?room=<ROOM>&token=<TOKEN> でこのページを開き、画面共有を開始してください。", de: "Öffnen Sie diese Seite mit /host?room=<ROOM>&token=<TOKEN> und starten Sie dann die Bildschirmfreigabe.", ru: "Откройте эту страницу по адресу /host?room=<ROOM>&token=<TOKEN>, затем начните показ экрана." },
    host_install_hint: { en: "Installing the host or opening fullscreen removes most browser chrome and feels closer to a dedicated sender app.", "zh-CN": "安装 Host 或进入全屏后，大部分浏览器外壳都会消失，体验更像专用发送端应用。", "zh-TW": "安裝 Host 或進入全螢幕後，大部分瀏覽器外殼都會消失，體驗更像專用傳送端 App。", ko: "호스트를 설치하거나 전체 화면으로 전환하면 브라우저 UI가 대부분 사라져 전용 송신 앱처럼 느껴집니다.", ja: "ホストをインストールするか全画面にすると、ブラウザーの装飾がほぼ消え、専用送信アプリに近い見た目になります。", de: "Durch Installation des Hosts oder Vollbild verschwindet der meiste Browser-Chrome und es wirkt eher wie eine dedizierte Sender-App.", ru: "Если установить хост или перейти в полноэкранный режим, большая часть браузерной оболочки исчезнет, и всё будет похоже на отдельное приложение-передатчик." },
    viewer_hint: { en: "Opening in fullscreen or installed app mode hides most browser chrome and feels closer to a native receiver.", "zh-CN": "进入全屏或已安装应用模式后，大部分浏览器外壳会被隐藏，体验更像原生接收端。", "zh-TW": "進入全螢幕或已安裝 App 模式後，大部分瀏覽器外殼會被隱藏，體驗更像原生接收端。", ko: "전체 화면이나 설치된 앱 모드에서는 브라우저 UI가 대부분 사라져 원래 앱처럼 보입니다.", ja: "全画面またはインストール済みアプリモードでは、ブラウザーの装飾がほぼ消え、ネイティブ受信アプリに近くなります。", de: "Vollbild oder installierter App-Modus blendet den meisten Browser-Chrome aus und wirkt eher wie ein nativer Empfänger.", ru: "В полноэкранном режиме или в установленном приложении большая часть браузерной оболочки скрывается, и всё выглядит как нативный клиент." },
    tap_to_play: { en: "Tap To Play", "zh-CN": "点击播放", "zh-TW": "點擊播放", ko: "탭해서 재생", ja: "タップして再生", de: "Tippen zum Abspielen", ru: "Нажмите для воспроизведения" },
  });

  Object.keys(CATALOG).forEach((key) => {
    Object.keys(CATALOG[key]).forEach((locale) => {
      REVERSE.set(CATALOG[key][locale], key);
    });
  });

  const state = {
    locale: detectLocale(),
    observer: null,
  };

  function translateByPattern(text, locale) {
    let match = text.match(/^LAN Host - (.+)$/);
    if (match) {
      const room = match[1];
      if (locale === "zh-CN") return `局域网主控端 - ${room}`;
      if (locale === "zh-TW") return `區域網主控端 - ${room}`;
      if (locale === "ko") return `LAN 호스트 - ${room}`;
      if (locale === "ja") return `LAN ホスト - ${room}`;
      if (locale === "de") return `LAN-Host - ${room}`;
      if (locale === "ru") return `LAN-хост - ${room}`;
    }
    match = text.match(/^LAN Viewer - (.+)$/);
    if (match) {
      const room = match[1];
      if (locale === "zh-CN") return `局域网接收端 - ${room}`;
      if (locale === "zh-TW") return `區域網接收端 - ${room}`;
      if (locale === "ko") return `LAN 뷰어 - ${room}`;
      if (locale === "ja") return `LAN ビューアー - ${room}`;
      if (locale === "de") return `LAN-Viewer - ${room}`;
      if (locale === "ru") return `LAN Viewer - ${room}`;
    }
    return text;
  }

  function translateText(text, locale) {
    const target = normalize(locale || state.locale);
    const value = String(text || "");
    if (!value.trim()) return value;
    const leading = value.match(/^\s*/)[0];
    const trailing = value.match(/\s*$/)[0];
    const core = value.trim();
    const key = REVERSE.get(core);
    if (key && CATALOG[key] && CATALOG[key][target]) {
      return leading + CATALOG[key][target] + trailing;
    }
    return leading + translateByPattern(core, target) + trailing;
  }

  function shouldSkipElement(el) {
    return !!(el && (el.closest("script,style,pre,code,samp,kbd,textarea,[data-no-i18n]") || el.tagName === "OPTION"));
  }

  function translateTextNode(node) {
    if (!node || !node.parentElement || shouldSkipElement(node.parentElement)) return;
    const translated = translateText(node.nodeValue, state.locale);
    if (translated !== node.nodeValue) {
      node.nodeValue = translated;
    }
  }

  function translateAttributes(root) {
    const nodes = [];
    if (root && root.nodeType === Node.ELEMENT_NODE) nodes.push(root);
    if (root && root.querySelectorAll) nodes.push(...root.querySelectorAll("[title],[placeholder],[aria-label]"));
    nodes.forEach((el) => {
      if (shouldSkipElement(el)) return;
      ["title", "placeholder", "aria-label"].forEach((attr) => {
        if (!el.hasAttribute(attr)) return;
        const current = el.getAttribute(attr) || "";
        const translated = translateText(current, state.locale);
        if (translated !== current) {
          el.setAttribute(attr, translated);
        }
      });
    });
  }

  function syncSelects() {
    document.querySelectorAll("[data-language-select]").forEach((select) => {
      if (!select.options.length) {
        SUPPORTED.forEach((item) => {
          const option = document.createElement("option");
          option.value = item.code;
          option.textContent = item.label;
          select.appendChild(option);
        });
      }
      if (select.value !== state.locale) {
        select.value = state.locale;
      }
    });
  }

  function apply(root) {
    const target = root && root.nodeType ? root : document.body;
    if (!target) return;
    if (target.nodeType === Node.TEXT_NODE) {
      translateTextNode(target);
    } else {
      const walker = document.createTreeWalker(target, NodeFilter.SHOW_TEXT);
      let node = walker.nextNode();
      while (node) {
        translateTextNode(node);
        node = walker.nextNode();
      }
      translateAttributes(target);
    }
    document.documentElement.lang = state.locale;
    document.title = translateText(document.title, state.locale);
    const appleTitle = document.querySelector('meta[name="apple-mobile-web-app-title"]');
    if (appleTitle) {
      appleTitle.setAttribute("content", translateText(appleTitle.getAttribute("content") || "", state.locale));
    }
    syncSelects();
  }

  function watch() {
    if (state.observer || !document.documentElement) return;
    state.observer = new MutationObserver((mutations) => {
      mutations.forEach((mutation) => {
        if (mutation.type === "characterData") {
          translateTextNode(mutation.target);
          return;
        }
        if (mutation.type === "attributes") {
          translateAttributes(mutation.target);
          return;
        }
        mutation.addedNodes.forEach((node) => apply(node));
      });
    });
    state.observer.observe(document.documentElement, {
      childList: true,
      subtree: true,
      characterData: true,
      attributes: true,
      attributeFilter: ["title", "placeholder", "aria-label"],
    });
  }

  function setLocale(locale, options) {
    const settings = options || {};
    state.locale = normalize(locale || state.locale);
    try {
      if (settings.persist !== false) {
        localStorage.setItem(STORAGE_KEY, state.locale);
      }
    } catch {}
    apply(document.body || document.documentElement);
    return state.locale;
  }

  function init(locale) {
    setLocale(locale || state.locale, { persist: locale !== undefined });
    watch();
  }

  window.LanShareI18n = {
    init,
    setLocale,
    getLocale: () => state.locale,
    translateText,
    applyDocument: () => apply(document.body || document.documentElement),
    supportedLocales: SUPPORTED.slice(),
    storageKey: STORAGE_KEY,
  };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => init());
  } else {
    init();
  }
})();
