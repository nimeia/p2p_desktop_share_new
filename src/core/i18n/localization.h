#pragma once

#include <array>
#include <codecvt>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace lan::i18n {

struct NativeTranslationEntry {
  std::wstring_view key;
  std::wstring_view en;
  std::wstring_view zhCN;
  std::wstring_view zhTW;
  std::wstring_view ko;
  std::wstring_view ja;
  std::wstring_view de;
  std::wstring_view ru;
};

inline std::wstring ToLowerAscii(std::wstring_view value) {
  std::wstring out;
  out.reserve(value.size());
  for (wchar_t ch : value) {
    if (ch >= L'A' && ch <= L'Z') {
      out.push_back(static_cast<wchar_t>(ch - L'A' + L'a'));
    } else if (ch == L'_') {
      out.push_back(L'-');
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

inline std::wstring NormalizeLocaleCode(std::wstring_view raw) {
  const std::wstring lower = ToLowerAscii(raw);
  if (lower.empty()) return L"en";

  if (lower == L"en" || lower.rfind(L"en-", 0) == 0) return L"en";
  if (lower == L"zh" || lower == L"zh-cn" || lower == L"zh-sg" || lower == L"zh-hans" ||
      lower.rfind(L"zh-cn-", 0) == 0 || lower.rfind(L"zh-sg-", 0) == 0 || lower.rfind(L"zh-hans-", 0) == 0) {
    return L"zh-CN";
  }
  if (lower == L"zh-tw" || lower == L"zh-hk" || lower == L"zh-mo" || lower == L"zh-hant" ||
      lower.rfind(L"zh-tw-", 0) == 0 || lower.rfind(L"zh-hk-", 0) == 0 || lower.rfind(L"zh-mo-", 0) == 0 ||
      lower.rfind(L"zh-hant-", 0) == 0) {
    return L"zh-TW";
  }
  if (lower == L"ko" || lower.rfind(L"ko-", 0) == 0) return L"ko";
  if (lower == L"ja" || lower.rfind(L"ja-", 0) == 0) return L"ja";
  if (lower == L"de" || lower.rfind(L"de-", 0) == 0) return L"de";
  if (lower == L"ru" || lower.rfind(L"ru-", 0) == 0) return L"ru";
  return L"en";
}

inline bool IsSupportedLocale(std::wstring_view value) {
  const std::wstring normalized = NormalizeLocaleCode(value);
  return normalized == L"en" || normalized == L"zh-CN" || normalized == L"zh-TW" ||
         normalized == L"ko" || normalized == L"ja" || normalized == L"de" || normalized == L"ru";
}

inline const std::array<std::wstring_view, 7>& SupportedLocales() {
  static const std::array<std::wstring_view, 7> kLocales = {
      L"en",
      L"zh-CN",
      L"zh-TW",
      L"ko",
      L"ja",
      L"de",
      L"ru",
  };
  return kLocales;
}

inline std::wstring LookupEnvLocale(const char* name) {
  const char* value = std::getenv(name);
  if (!value || !*value) return L"";
  std::wstring out;
  while (*value) {
    out.push_back(static_cast<unsigned char>(*value));
    ++value;
  }
  return NormalizeLocaleCode(out);
}

inline std::wstring DetectSystemLocaleCode() {
#if defined(_WIN32)
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
  if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
    return NormalizeLocaleCode(localeName);
  }
#endif

  for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    const std::wstring locale = LookupEnvLocale(name);
    if (!locale.empty()) return locale;
  }
  return L"en";
}

inline std::filesystem::path LocalePreferencePath() {
#if defined(_WIN32)
  wchar_t localAppData[MAX_PATH]{};
  const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
  if (len > 0 && len < MAX_PATH) {
    return std::filesystem::path(localAppData) / L"LanScreenShareHostApp" / L"settings" / L"locale.txt";
  }
#endif
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::filesystem::path(home) / ".config" / "lan-screenshare" / "locale.txt";
  }
  return std::filesystem::temp_directory_path() / "lan-screenshare-locale.txt";
}

inline std::wstring LoadPreferredLocale() {
  const auto path = LocalePreferencePath();
  std::ifstream in(path, std::ios::binary);
  if (in) {
    std::string raw;
    std::getline(in, raw);
    if (!raw.empty()) {
      std::wstring value;
      value.reserve(raw.size());
      for (unsigned char ch : raw) value.push_back(static_cast<wchar_t>(ch));
      const std::wstring normalized = NormalizeLocaleCode(value);
      if (IsSupportedLocale(normalized)) return normalized;
    }
  }
  return DetectSystemLocaleCode();
}

inline bool SavePreferredLocale(std::wstring_view locale, std::wstring* err = nullptr) {
  const std::wstring normalized = NormalizeLocaleCode(locale);
  if (!IsSupportedLocale(normalized)) {
    if (err) *err = L"Unsupported locale code.";
    return false;
  }

  const auto path = LocalePreferencePath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    if (err) {
      const std::string message = ec.message();
      *err = std::wstring(message.begin(), message.end());
    }
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (err) *err = L"Failed to open locale preference file.";
    return false;
  }

  std::string narrow;
  narrow.reserve(normalized.size());
  for (wchar_t ch : normalized) narrow.push_back(static_cast<char>(ch));
  out << narrow;
  if (!out) {
    if (err) *err = L"Failed to write locale preference file.";
    return false;
  }

  if (err) err->clear();
  return true;
}

inline std::wstring AppendLocaleQuery(std::wstring_view url, std::wstring_view locale) {
  const std::wstring normalized = NormalizeLocaleCode(locale);
  if (!IsSupportedLocale(normalized) || url.empty()) return std::wstring(url);

  std::wstring base(url);
  std::wstring fragment;
  const std::size_t hashPos = base.find(L'#');
  if (hashPos != std::wstring::npos) {
    fragment = base.substr(hashPos);
    base.resize(hashPos);
  }

  auto removeExisting = [](std::wstring& value) {
    const std::wstring needle = L"lang=";
    std::size_t queryPos = value.find(L'?');
    if (queryPos == std::wstring::npos) return;

    std::wstring rebuilt = value.substr(0, queryPos + 1);
    std::wstring query = value.substr(queryPos + 1);
    std::size_t pos = 0;
    bool first = true;
    while (pos <= query.size()) {
      const std::size_t amp = query.find(L'&', pos);
      const std::wstring part = query.substr(pos, amp == std::wstring::npos ? std::wstring::npos : amp - pos);
      if (!part.empty() && part.rfind(needle, 0) != 0) {
        if (!first) rebuilt.push_back(L'&');
        rebuilt += part;
        first = false;
      }
      if (amp == std::wstring::npos) break;
      pos = amp + 1;
    }
    if (!rebuilt.empty() && rebuilt.back() == L'?') {
      rebuilt.pop_back();
    }
    value = std::move(rebuilt);
  };

  removeExisting(base);
  base += (base.find(L'?') == std::wstring::npos ? L"?" : L"&");
  base += L"lang=";
  base += normalized;
  return base + fragment;
}

inline const NativeTranslationEntry* FindNativeTranslation(std::wstring_view key) {
  static const NativeTranslationEntry kEntries[] = {
      {L"Open Dashboard", L"Open Dashboard", L"打开控制台", L"開啟控制台", L"대시보드 열기", L"ダッシュボードを開く", L"Dashboard öffnen", L"Открыть панель"},
      {L"Refresh Dashboard", L"Refresh Dashboard", L"刷新控制台", L"重新整理控制台", L"대시보드 새로 고침", L"ダッシュボードを更新", L"Dashboard aktualisieren", L"Обновить панель"},
      {L"Refresh Dashboard (service stopped)", L"Refresh Dashboard (service stopped)", L"刷新控制台（服务未运行）", L"重新整理控制台（服務未執行）", L"대시보드 새로 고침(서비스 중지됨)", L"ダッシュボードを更新（サービス停止中）", L"Dashboard aktualisieren (Dienst angehalten)", L"Обновить панель (служба остановлена)"},
      {L"Open Viewer URL", L"Open Viewer URL", L"打开接收端链接", L"開啟接收端連結", L"뷰어 URL 열기", L"閲覧用 URL を開く", L"Viewer-URL öffnen", L"Открыть URL для просмотра"},
      {L"Open Viewer URL (1 viewer)", L"Open Viewer URL (1 viewer)", L"打开接收端链接（1 个接收方）", L"開啟接收端連結（1 位接收方）", L"뷰어 URL 열기(1명 시청 중)", L"閲覧用 URL を開く（1 人接続中）", L"Viewer-URL öffnen (1 Zuschauer)", L"Открыть URL для просмотра (1 зритель)"},
      {L"Start Sharing Service", L"Start Sharing Service", L"启动共享服务", L"啟動共享服務", L"공유 서비스 시작", L"共有サービスを開始", L"Freigabedienst starten", L"Запустить службу общего доступа"},
      {L"Start Sharing Service (already running)", L"Start Sharing Service (already running)", L"启动共享服务（已在运行）", L"啟動共享服務（已在執行）", L"공유 서비스 시작(이미 실행 중)", L"共有サービスを開始（すでに実行中）", L"Freigabedienst starten (läuft bereits)", L"Запустить службу общего доступа (уже работает)"},
      {L"Stop Sharing Service", L"Stop Sharing Service", L"停止共享服务", L"停止共享服務", L"공유 서비스 중지", L"共有サービスを停止", L"Freigabedienst stoppen", L"Остановить службу общего доступа"},
      {L"Stop Sharing Service (not running)", L"Stop Sharing Service (not running)", L"停止共享服务（未运行）", L"停止共享服務（未執行）", L"공유 서비스 중지(실행 중 아님)", L"共有サービスを停止（未実行）", L"Freigabedienst stoppen (läuft nicht)", L"Остановить службу общего доступа (не запущена)"},
      {L"Copy Viewer URL", L"Copy Viewer URL", L"复制接收端链接", L"複製接收端連結", L"뷰어 URL 복사", L"閲覧用 URL をコピー", L"Viewer-URL kopieren", L"Скопировать URL для просмотра"},
      {L"Show QR / Share Card", L"Show QR / Share Card", L"显示二维码 / 共享卡片", L"顯示 QR Code / 分享卡片", L"QR / 공유 카드 표시", L"QR / 共有カードを表示", L"QR / Freigabekarte anzeigen", L"Показать QR / карточку доступа"},
      {L"Open Share Wizard", L"Open Share Wizard", L"打开共享向导", L"開啟分享精靈", L"공유 마법사 열기", L"共有ウィザードを開く", L"Freigabeassistent öffnen", L"Открыть мастер доступа"},
      {L"Open Diagnostics Folder", L"Open Diagnostics Folder", L"打开诊断文件夹", L"開啟診斷資料夾", L"진단 폴더 열기", L"診断フォルダーを開く", L"Diagnoseordner öffnen", L"Открыть папку диагностики"},
      {L"Open Diagnostics Folder (service stopped)", L"Open Diagnostics Folder (service stopped)", L"打开诊断文件夹（服务未运行）", L"開啟診斷資料夾（服務未執行）", L"진단 폴더 열기(서비스 중지됨)", L"診断フォルダーを開く（サービス停止中）", L"Diagnoseordner öffnen (Dienst angehalten)", L"Открыть папку диагностики (служба остановлена)"},
      {L"Export Diagnostics Snapshot", L"Export Diagnostics Snapshot", L"导出诊断快照", L"匯出診斷快照", L"진단 스냅샷 내보내기", L"診断スナップショットをエクスポート", L"Diagnose-Snapshot exportieren", L"Экспортировать снимок диагностики"},
      {L"Export Diagnostics Snapshot (no live data yet)", L"Export Diagnostics Snapshot (no live data yet)", L"导出诊断快照（暂无实时数据）", L"匯出診斷快照（尚無即時資料）", L"진단 스냅샷 내보내기(실시간 데이터 없음)", L"診断スナップショットをエクスポート（まだライブデータなし）", L"Diagnose-Snapshot exportieren (noch keine Live-Daten)", L"Экспортировать снимок диагностики (пока нет данных)"},
      {L"Failed to export diagnostics snapshot.", L"Failed to export diagnostics snapshot.", L"导出诊断快照失败。", L"匯出診斷快照失敗。", L"진단 스냅샷 내보내기에 실패했습니다.", L"診断スナップショットのエクスポートに失敗しました。", L"Diagnose-Snapshot konnte nicht exportiert werden.", L"Не удалось экспортировать снимок диагностики."},
      {L"Quit", L"Quit", L"退出", L"結束", L"종료", L"終了", L"Beenden", L"Выход"},
      {L"Status: starting", L"Status: starting", L"状态：启动中", L"狀態：啟動中", L"상태: 시작 중", L"状態: 起動中", L"Status: startet", L"Статус: запуск"},
      {L"Waiting for first refresh...", L"Waiting for first refresh...", L"等待首次刷新...", L"等待首次重新整理...", L"첫 새로 고침 대기 중...", L"最初の更新を待機中...", L"Warten auf erste Aktualisierung...", L"Ожидание первого обновления..."},
      {L"Healthy", L"Healthy", L"健康", L"健康", L"정상", L"正常", L"In Ordnung", L"Исправно"},
      {L"Health degraded", L"Health degraded", L"健康异常", L"健康異常", L"상태 저하", L"状態低下", L"Gesundheit beeinträchtigt", L"Состояние ухудшено"},
      {L"Service stopped", L"Service stopped", L"服务已停止", L"服務已停止", L"서비스 중지됨", L"サービス停止", L"Dienst gestoppt", L"Служба остановлена"},
      {L"LAN Screen Share Host", L"LAN Screen Share Host", L"局域网屏幕共享主机", L"區域網路螢幕分享主機", L"LAN 화면 공유 호스트", L"LAN 画面共有ホスト", L"LAN-Bildschirmfreigabe Host", L"Хост LAN Screen Share"},
      {L"Attention needed", L"Attention needed", L"需要关注", L"需要注意", L"주의 필요", L"要対応", L"Aufmerksamkeit erforderlich", L"Требуется внимание"},
      {L"The local sharing service is stopped.", L"The local sharing service is stopped.", L"本地共享服务已停止。", L"本地共享服務已停止。", L"로컬 공유 서비스가 중지되었습니다.", L"ローカル共有サービスは停止しています。", L"Der lokale Freigabedienst ist angehalten.", L"Локальная служба общего доступа остановлена."},
      {L"Waiting for viewers.", L"Waiting for viewers.", L"等待接收方连接。", L"等待接收方連線。", L"뷰어 연결 대기 중입니다.", L"閲覧側の接続を待機しています。", L"Warten auf Viewer.", L"Ожидание подключений зрителей."},
      {L"Ready", L"Ready", L"就绪", L"就緒", L"준비됨", L"準備完了", L"Bereit", L"Готово"},
      {L"Sharing", L"Sharing", L"共享中", L"分享中", L"공유 중", L"共有中", L"Freigabe aktiv", L"Идет трансляция"},
      {L"Stopped", L"Stopped", L"已停止", L"已停止", L"중지됨", L"停止", L"Gestoppt", L"Остановлено"},
      {L"Needs attention", L"Needs attention", L"需要处理", L"需要處理", L"처리 필요", L"要確認", L"Benötigt Aufmerksamkeit", L"Требует внимания"},
      {L"Running in tray", L"Running in tray", L"正在后台托盘运行", L"正在背景系統匣執行", L"트레이에서 실행 중", L"トレイで実行中", L"Läuft im Infobereich", L"Работает в трее"},
      {L"The window can stay hidden while the local sharing service remains available.", L"The window can stay hidden while the local sharing service remains available.", L"窗口可以保持隐藏，本地共享服务仍会继续提供。", L"視窗可維持隱藏，本地共享服務仍會持續提供。", L"창을 숨겨도 로컬 공유 서비스는 계속 제공됩니다.", L"ウィンドウを非表示のままでもローカル共有サービスは利用できます。", L"Das Fenster kann verborgen bleiben, während der lokale Freigabedienst verfügbar bleibt.", L"Окно может оставаться скрытым, пока локальная служба общего доступа продолжает работать."},
      {L"Dashboard refreshed", L"Dashboard refreshed", L"控制台已刷新", L"控制台已重新整理", L"대시보드가 새로 고쳐졌습니다", L"ダッシュボードを更新しました", L"Dashboard aktualisiert", L"Панель обновлена"},
      {L"The dashboard live probe completed successfully.", L"The dashboard live probe completed successfully.", L"控制台实时探测已成功完成。", L"控制台即時探測已成功完成。", L"대시보드 라이브 점검이 완료되었습니다.", L"ダッシュボードのライブ確認が完了しました。", L"Die Live-Prüfung des Dashboards wurde erfolgreich abgeschlossen.", L"Проверка панели в реальном времени успешно завершена."},
      {L"Sharing service started", L"Sharing service started", L"共享服务已启动", L"共享服務已啟動", L"공유 서비스가 시작되었습니다", L"共有サービスを開始しました", L"Freigabedienst gestartet", L"Служба общего доступа запущена"},
      {L"The native sharing service is live and healthy.", L"The native sharing service is live and healthy.", L"本地共享服务已启动并运行正常。", L"本地共享服務已啟動且運作正常。", L"네이티브 공유 서비스가 정상적으로 실행 중입니다.", L"ネイティブ共有サービスは正常に稼働しています。", L"Der native Freigabedienst läuft und ist gesund.", L"Встроенная служба общего доступа запущена и работает нормально."},
      {L"Sharing service stopped", L"Sharing service stopped", L"共享服务已停止", L"共享服務已停止", L"공유 서비스가 중지되었습니다", L"共有サービスを停止しました", L"Freigabedienst gestoppt", L"Служба общего доступа остановлена"},
      {L"The native sharing service has stopped.", L"The native sharing service has stopped.", L"本地共享服务已经停止。", L"本地共享服務已經停止。", L"네이티브 공유 서비스가 중지되었습니다.", L"ネイティブ共有サービスは停止しました。", L"Der native Freigabedienst wurde gestoppt.", L"Встроенная служба общего доступа остановлена."},
      {L"Diagnostics exported", L"Diagnostics exported", L"诊断已导出", L"診斷已匯出", L"진단을 내보냈습니다", L"診断をエクスポートしました", L"Diagnose exportiert", L"Диагностика экспортирована"},
      {L"Action failed", L"Action failed", L"操作失败", L"操作失敗", L"작업 실패", L"操作に失敗しました", L"Aktion fehlgeschlagen", L"Ошибка действия"},
      {L"The requested action failed.", L"The requested action failed.", L"请求的操作失败。", L"要求的操作失敗。", L"요청한 작업이 실패했습니다.", L"要求された操作に失敗しました。", L"Die angeforderte Aktion ist fehlgeschlagen.", L"Запрошенное действие завершилось ошибкой."},
      {L"The action completed successfully.", L"The action completed successfully.", L"操作已成功完成。", L"操作已成功完成。", L"작업이 성공적으로 완료되었습니다.", L"操作が正常に完了しました。", L"Die Aktion wurde erfolgreich abgeschlossen.", L"Действие успешно выполнено."},
      {L"Running outside WebView2", L"Running outside WebView2", L"当前在 WebView2 外运行", L"目前在 WebView2 外執行", L"현재 WebView2 외부에서 실행 중", L"現在 WebView2 の外部で実行中", L"Derzeit außerhalb von WebView2 ausgeführt", L"Сейчас работает вне WebView2"},
      {L"Language", L"Language", L"语言", L"語言", L"언어", L"言語", L"Sprache", L"Язык"},
      {L"English", L"English", L"英文", L"英文", L"영어", L"英語", L"Englisch", L"Английский"},
      {L"Simplified Chinese", L"Simplified Chinese", L"简体中文", L"簡體中文", L"중국어 간체", L"中国語（簡体字）", L"Chinesisch (vereinfacht)", L"Китайский (упрощенный)"},
      {L"Traditional Chinese", L"Traditional Chinese", L"繁体中文", L"繁體中文", L"중국어 번체", L"中国語（繁体字）", L"Chinesisch (traditionell)", L"Китайский (традиционный)"},
      {L"Korean", L"Korean", L"韩文", L"韓文", L"한국어", L"韓国語", L"Koreanisch", L"Корейский"},
      {L"Japanese", L"Japanese", L"日文", L"日文", L"일본어", L"日本語", L"Japanisch", L"Японский"},
      {L"German", L"German", L"德语", L"德語", L"독일어", L"ドイツ語", L"Deutsch", L"Немецкий"},
      {L"Russian", L"Russian", L"俄语", L"俄語", L"러시아어", L"ロシア語", L"Russisch", L"Русский"},
  };

  for (const auto& entry : kEntries) {
    if (entry.key == key) return &entry;
  }
  return nullptr;
}

inline bool EndsWith(std::wstring_view value, std::wstring_view suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::wstring TranslateEntry(const NativeTranslationEntry& entry, std::wstring_view locale) {
  const std::wstring normalized = NormalizeLocaleCode(locale);
  if (normalized == L"zh-CN") return std::wstring(entry.zhCN);
  if (normalized == L"zh-TW") return std::wstring(entry.zhTW);
  if (normalized == L"ko") return std::wstring(entry.ko);
  if (normalized == L"ja") return std::wstring(entry.ja);
  if (normalized == L"de") return std::wstring(entry.de);
  if (normalized == L"ru") return std::wstring(entry.ru);
  return std::wstring(entry.en);
}

inline std::wstring TranslateNativeText(std::wstring_view source, std::wstring_view locale) {
  const std::wstring normalized = NormalizeLocaleCode(locale);
  if (normalized == L"en" || source.empty()) return std::wstring(source);

  if (const auto* entry = FindNativeTranslation(source)) {
    return TranslateEntry(*entry, normalized);
  }

  const std::wstring s(source);
  const std::wstring viewerPrefix = L"Open Viewer URL (";
  const std::wstring viewersSuffix = L" viewers)";
  if (s.rfind(viewerPrefix, 0) == 0 && s.size() > viewerPrefix.size() + viewersSuffix.size() &&
      EndsWith(s, viewersSuffix)) {
    const std::wstring count = s.substr(viewerPrefix.size(), s.size() - viewerPrefix.size() - viewersSuffix.size());
    if (normalized == L"zh-CN") return L"打开接收端链接（" + count + L" 个接收方）";
    if (normalized == L"zh-TW") return L"開啟接收端連結（" + count + L" 位接收方）";
    if (normalized == L"ko") return L"뷰어 URL 열기(" + count + L"명 시청 중)";
    if (normalized == L"ja") return L"閲覧用 URL を開く（" + count + L" 人接続中）";
    if (normalized == L"de") return L"Viewer-URL öffnen (" + count + L" Zuschauer)";
    if (normalized == L"ru") return L"Открыть URL для просмотра (" + count + L" зрителей)";
  }

  const std::wstring statusPrefix = L"Status: ";
  if (s == L"Status: running") {
    if (normalized == L"zh-CN") return L"状态：运行中";
    if (normalized == L"zh-TW") return L"狀態：執行中";
    if (normalized == L"ko") return L"상태: 실행 중";
    if (normalized == L"ja") return L"状態: 実行中";
    if (normalized == L"de") return L"Status: läuft";
    if (normalized == L"ru") return L"Статус: работает";
  }
  if (s == L"Status: stopped") {
    if (normalized == L"zh-CN") return L"状态：已停止";
    if (normalized == L"zh-TW") return L"狀態：已停止";
    if (normalized == L"ko") return L"상태: 중지됨";
    if (normalized == L"ja") return L"状態: 停止";
    if (normalized == L"de") return L"Status: angehalten";
    if (normalized == L"ru") return L"Статус: остановлено";
  }
  if (s.rfind(statusPrefix, 0) == 0) {
    const std::wstring rest = s.substr(statusPrefix.size());
    if (normalized == L"zh-CN") return L"状态： " + TranslateNativeText(rest, normalized);
    if (normalized == L"zh-TW") return L"狀態： " + TranslateNativeText(rest, normalized);
    if (normalized == L"ko") return L"상태: " + TranslateNativeText(rest, normalized);
    if (normalized == L"ja") return L"状態: " + TranslateNativeText(rest, normalized);
    if (normalized == L"de") return L"Status: " + TranslateNativeText(rest, normalized);
    if (normalized == L"ru") return L"Статус: " + TranslateNativeText(rest, normalized);
  }

  const std::wstring viewersConnectedSuffix = L" viewer(s) connected.";
  if (EndsWith(s, viewersConnectedSuffix)) {
    const std::wstring count = s.substr(0, s.size() - viewersConnectedSuffix.size());
    if (normalized == L"zh-CN") return count + L" 个接收方已连接。";
    if (normalized == L"zh-TW") return count + L" 位接收方已連線。";
    if (normalized == L"ko") return count + L"명의 뷰어가 연결되었습니다.";
    if (normalized == L"ja") return count + L" 人の閲覧側が接続しました。";
    if (normalized == L"de") return count + L" Viewer verbunden.";
    if (normalized == L"ru") return L"Подключено зрителей: " + count + L".";
  }

  const std::wstring viewerBadgeSuffix = L" viewer(s)";
  if (EndsWith(s, viewerBadgeSuffix)) {
    const std::wstring count = s.substr(0, s.size() - viewerBadgeSuffix.size());
    if (normalized == L"zh-CN") return count + L" 个接收方";
    if (normalized == L"zh-TW") return count + L" 位接收方";
    if (normalized == L"ko") return count + L"명 시청 중";
    if (normalized == L"ja") return count + L" 人接続";
    if (normalized == L"de") return count + L" Zuschauer";
    if (normalized == L"ru") return count + L" зрителей";
  }

  const std::wstring tooltipPrefix = L"LAN Screen Share Host - ";
  if (s.rfind(tooltipPrefix, 0) == 0) {
    const std::wstring rest = s.substr(tooltipPrefix.size());
    const std::wstring translatedRest = TranslateNativeText(rest, normalized);
    if (!translatedRest.empty() && translatedRest != rest) {
      return TranslateNativeText(L"LAN Screen Share Host", normalized) + L" - " + translatedRest;
    }
  }

  if (s == L"Ready to share") {
    if (normalized == L"zh-CN") return L"可开始共享";
    if (normalized == L"zh-TW") return L"可開始分享";
    if (normalized == L"ko") return L"공유 준비 완료";
    if (normalized == L"ja") return L"共有の準備完了";
    if (normalized == L"de") return L"Bereit zum Teilen";
    if (normalized == L"ru") return L"Готово к показу";
  }
  if (s == L"Sharing (waiting for viewers)") {
    if (normalized == L"zh-CN") return L"共享中（等待接收方）";
    if (normalized == L"zh-TW") return L"分享中（等待接收方）";
    if (normalized == L"ko") return L"공유 중(뷰어 대기 중)";
    if (normalized == L"ja") return L"共有中（閲覧側を待機中）";
    if (normalized == L"de") return L"Freigabe aktiv (wartet auf Viewer)";
    if (normalized == L"ru") return L"Идет трансляция (ожидание зрителей)";
  }
  const std::wstring sharingWithViewersPrefix = L"Sharing (";
  if (s.rfind(sharingWithViewersPrefix, 0) == 0 && EndsWith(s, viewerBadgeSuffix + L")")) {
    const std::wstring count = s.substr(sharingWithViewersPrefix.size(), s.size() - sharingWithViewersPrefix.size() - viewerBadgeSuffix.size() - 1);
    if (normalized == L"zh-CN") return L"共享中（" + count + L" 个接收方）";
    if (normalized == L"zh-TW") return L"分享中（" + count + L" 位接收方）";
    if (normalized == L"ko") return L"공유 중(" + count + L"명 시청 중)";
    if (normalized == L"ja") return L"共有中（" + count + L" 人接続中）";
    if (normalized == L"de") return L"Freigabe aktiv (" + count + L" Zuschauer)";
    if (normalized == L"ru") return L"Идет трансляция (" + count + L" зрителей)";
  }

  return s;
}

inline std::string NarrowAscii(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

inline std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(value.data(), value.data() + value.size());
}

inline std::string TranslateNativeTextUtf8(std::wstring_view source, std::wstring_view locale) {
  return WideToUtf8(TranslateNativeText(source, locale));
}

} // namespace lan::i18n
