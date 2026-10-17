// Copyright TapTap. All Rights Reserved.

#include "TapLoginStrings.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"

// 参照 Unity LoginLanguage.cs, 每个语言对应 ETapLanguage 枚举值
// Auto(0) 根据系统语言自动检测

// 检测系统语言 (Auto 模式时使用)
// 使用 FPlatformMisc::GetDefaultLanguage() 获取 OS 级语言,
// 而非 FInternationalization (UE5 引擎默认是英文)
static ETapLanguage DetectSystemLanguage()
{
	// 返回如 "zh-Hans", "en", "ja", "ko" 等
	FString OsLang = FPlatformMisc::GetDefaultLanguage();
	UE_LOG(LogTemp, Log, TEXT("[TapLogin] OS default language: %s"), *OsLang);

	if (OsLang.StartsWith(TEXT("zh")))
	{
		if (OsLang.Contains(TEXT("Hant")) || OsLang.Contains(TEXT("TW")) || OsLang.Contains(TEXT("HK")))
			return ETapLanguage::zh_Hant;
		return ETapLanguage::zh_Hans;
	}
	if (OsLang.StartsWith(TEXT("ja"))) return ETapLanguage::ja;
	if (OsLang.StartsWith(TEXT("ko"))) return ETapLanguage::ko;
	if (OsLang.StartsWith(TEXT("th"))) return ETapLanguage::th;
	if (OsLang.StartsWith(TEXT("id"))) return ETapLanguage::id;
	if (OsLang.StartsWith(TEXT("de"))) return ETapLanguage::de;
	if (OsLang.StartsWith(TEXT("es"))) return ETapLanguage::es;
	if (OsLang.StartsWith(TEXT("fr"))) return ETapLanguage::fr;
	if (OsLang.StartsWith(TEXT("pt"))) return ETapLanguage::pt;
	if (OsLang.StartsWith(TEXT("ru"))) return ETapLanguage::ru;
	if (OsLang.StartsWith(TEXT("tr"))) return ETapLanguage::tr;
	if (OsLang.StartsWith(TEXT("vi"))) return ETapLanguage::vi;
	return ETapLanguage::en;
}

// 辅助宏: 按语言 switch 返回字符串
#define TAP_LOGIN_STRING(ZH, EN, ZHTW, JA, KO, TH, ID, DE, ES, FR, PT, RU, TR, VI) \
	{ \
		ETapLanguage Lang = FTapSdkConfigState::GetCurrentConfig().PreferredLanguage; \
		if (Lang == ETapLanguage::Auto) Lang = DetectSystemLanguage(); \
		switch (Lang) \
		{ \
		case ETapLanguage::zh_Hans: return TEXT(ZH); \
		case ETapLanguage::en:      return TEXT(EN); \
		case ETapLanguage::zh_Hant: return TEXT(ZHTW); \
		case ETapLanguage::ja:      return TEXT(JA); \
		case ETapLanguage::ko:      return TEXT(KO); \
		case ETapLanguage::th:      return TEXT(TH); \
		case ETapLanguage::id:      return TEXT(ID); \
		case ETapLanguage::de:      return TEXT(DE); \
		case ETapLanguage::es:      return TEXT(ES); \
		case ETapLanguage::fr:      return TEXT(FR); \
		case ETapLanguage::pt:      return TEXT(PT); \
		case ETapLanguage::ru:      return TEXT(RU); \
		case ETapLanguage::tr:      return TEXT(TR); \
		case ETapLanguage::vi:      return TEXT(VI); \
		default:                    return TEXT(EN); \
		} \
	}

FString FTapLoginStrings::TitleLogin()
TAP_LOGIN_STRING(
	"使用 TapTap 账号登录",
	"Log In with TapTap",
	"使用 TapTap 登入",
	"TapTapでログイン",
	"Taptap으로 로그인",
	"ล็อกอินด้วย TapTap",
	"Log In dengan TapTap",
	"Anmeldung mit TapTap",
	"Iniciar sesión con TapTap",
	"Se connecter avec TapTap",
	"Entrar com TapTap",
	"Войти с TapTap",
	"TapTap ile Giriş Yap",
	"Đăng nhập bằng TapTap"
)

FString FTapLoginStrings::QrTitleLogin()
TAP_LOGIN_STRING(
	"安全扫码登录",
	"Via QR Code",
	"透過 QR 碼",
	"QRコードを使う",
	"QR 코드 사용",
	"ผ่าน QR โค้ด",
	"Melalui Kode QR",
	"Per QR-Code",
	"Mediante código QR",
	"Via le Code QR",
	"Via QR Code",
	"Через QR-код",
	"QR Kodu",
	"Dùng mã QR"
)

FString FTapLoginStrings::QrRefresh()
TAP_LOGIN_STRING(
	"点击刷新",
	"Refresh",
	"重新整理",
	"更新",
	"새로 고치기",
	"รีเฟรช",
	"Refresh",
	"Aktualisierung",
	"Actualizar",
	"Actualisé",
	"Atualizar",
	"Обновить",
	"Yenile",
	"Làm mới"
)

FString FTapLoginStrings::QrNoticeScanToLogin()
TAP_LOGIN_STRING(
	"请使用 TapTap 客户端\n扫描二维码登录",
	"Use TapTap App\nTo Scan The Code",
	"使用 TapTap 應用程式\n掃描 QR 碼",
	"TapTap アプリでQRコードをスキャン",
	"TapTap 앱으로 QR 코드를 스캔하세요",
	"สแกน QR โค้ดด้วยแอป TapTap",
	"Pindai kode QR dengan aplikasi TapTap",
	"Scanne den QR-Code mit der TapTap-App",
	"Escanee el código QR con la aplicación TapTap",
	"Scanner le Code QR avec l'application TapTap",
	"Escaneie o QR code com o aplicativo TapTap",
	"Просканируйте QR-код приложением TapTap",
	"TapTap uygulamasını kullanarak QR kodunu tara",
	"Quét mã QR bằng app TapTap"
)

FString FTapLoginStrings::WebLogin()
TAP_LOGIN_STRING(
	"使用网页浏览器完成授权",
	"Via Web Browser",
	"透過網路瀏覽器",
	"ウェブブラウザを使う",
	"웹 브라우저 사용",
	"ผ่านเว็บเบราว์เซอร์",
	"Melalui Browser Web",
	"Per Webbrowser",
	"A través del navegador web",
	"Via le Navigateur Web",
	"Através do Navegador da Web",
	"Через браузер",
	"İnternet Tarayıcısı",
	"Dùng Trình duyệt Web"
)

FString FTapLoginStrings::WebNotice()
TAP_LOGIN_STRING(
	"点击下方按钮前往网页浏览器，\n授权 TapTap 账号，完成后将自动返回游戏。",
	"Click the button below to open a web browser.\nYou'll be redirected back after login.",
	"點擊下方按鈕前往網頁瀏覽器，\n登入後系統會將你重新導回此頁面。",
	"ログイン後にリダイレクトされます。",
	"로그인 후에 원래 화면으로 돌아갑니다.",
	"คุณจะถูกเปลี่ยนเส้นทางกลับหลังจากล็อกอิน",
	"Kamu akan diarahkan kembali setelah log in.",
	"Du wirst nach der Anmeldung zurückgeleitet.",
	"Será redirigido de vuelta después de iniciar sesión.",
	"Vous serez redirigé après la connexion.",
	"Você será redirecionado de volta após o login.",
	"Вы будете перенаправлены после входа.",
	"Girişten sonra yeniden yönlendirileceksiniz.",
	"Bạn sẽ tự động quay lại sau khi đăng nhập."
)

FString FTapLoginStrings::WebButtonJumpToWeb()
TAP_LOGIN_STRING(
	"跳转至 TapTap",
	"Go to TapTap",
	"前往 TapTap",
	"TapTapを開く",
	"Taptap으로 이동",
	"ไปที่ TapTap",
	"Buka TapTap",
	"Gehe zu TapTap",
	"Ir a TapTap",
	"Aller à TapTap",
	"Ir para TapTap",
	"Перейти в TapTap",
	"TapTap",
	"Đến TapTap"
)

FString FTapLoginStrings::QrNoticeCancelTitle()
TAP_LOGIN_STRING("您已取消此次登录", "Login cancelled", "登入已取消", "ログインがキャンセルされました", "로그인 취소됨", "ยกเลิกการล็อกอินแล้ว", "Login dibatalkan", "Anmeldung abgebrochen", "Inicio de sesión cancelado", "Connexion annulée", "Login cancelado", "Вход отменён", "Giriş iptal edildi", "Đã hủy đăng nhập")

FString FTapLoginStrings::QrNoticeCancelDetail()
TAP_LOGIN_STRING("请重新扫码", "Please scan again", "請再次掃描", "もう一度スキャンしてください", "다시 스캔하세요", "โปรดสแกนอีกครั้ง", "Silakan pindai lagi", "Bitte erneut scannen", "Escanee de nuevo", "Veuillez scanner à nouveau", "Escaneie novamente", "Отсканируйте снова", "Lütfen tekrar tarayın", "Vui lòng quét lại")

FString FTapLoginStrings::QrNoticeSuccessTitle()
TAP_LOGIN_STRING("扫码成功", "Scan successful", "掃描成功", "スキャン成功", "스캔 성공", "สแกนสำเร็จ", "Pemindaian berhasil", "Scan erfolgreich", "Escaneo exitoso", "Scan réussi", "Escaneamento bem-sucedido", "Сканирование успешно", "Tarama başarılı", "Quét thành công")

FString FTapLoginStrings::QrNoticeSuccessDetail()
TAP_LOGIN_STRING("请在手机上确认", "Please confirm on your phone", "請在手機上確認", "スマホで確認してください", "스마트폰에서 확인하세요", "โปรดยืนยันบนโทรศัพท์", "Konfirmasi di ponsel", "Bitte auf dem Handy bestätigen", "Confirme en su teléfono", "Confirmez sur votre téléphone", "Confirme no seu telefone", "Подтвердите на телефоне", "Telefonunuzdan onaylayın", "Xác nhận trên điện thoại")

FString FTapLoginStrings::WebNoticeLoginTitle()
TAP_LOGIN_STRING("正在登录中，请稍后", "Logging in...", "正在登入……", "ログイン中…", "로그인 중...", "กำลังล็อกอิน...", "Sedang log in...", "Anmeldung...", "Iniciando sesión...", "Connexion...", "Fazendo login...", "Входите...", "Giriş yapılıyor…", "Đang đăng nhập…")

FString FTapLoginStrings::WebNoticeFailTitle()
TAP_LOGIN_STRING("登录失败", "Login failed", "登入失敗", "ログイン失敗", "로그인 실패", "ล็อกอินล้มเหลว", "Gagal log in", "Anmeldung fehlgeschlagen", "Error al iniciar sesión", "Échec de connexion", "Falha ao fazer login", "Не удалось войти", "Giriş yapılamadı", "Đăng nhập thất bại")

FString FTapLoginStrings::WebNoticeFailDetail()
TAP_LOGIN_STRING("请重新点击打开网页进行授权", "Please try again", "請再試一次", "もう一度お試しください", "다시 시도하세요", "โปรดลองอีกครั้ง", "Silakan coba lagi", "Versuche es erneut", "Inténtelo de nuevo", "Veuillez réessayer", "Tente novamente", "Попробуйте снова", "Lütfen tekrar deneyin", "Vui lòng thử lại")

#undef TAP_LOGIN_STRING

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
