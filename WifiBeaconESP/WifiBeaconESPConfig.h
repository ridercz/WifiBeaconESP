// HoneyESP firmware version
#define VERSION               "1.1.0"

// Network configuration
#define DNS_PORT              53
#define HTTP_PORT             80
#define AP_ADDRESS            "10.42.42.1"
#define AP_NETMASK            "255.255.255.0"
#define AP_MAX_CLIENTS        8
#define AP_SSID_FORMAT        "WifiBeacon-%06X"     // SSID used when no network name is specified in configuration, MAC is appended

// Names of system files in SPIFFS 
#define FILENAME_SYSTEM_CFG   "/system.cfg"
#define FILENAME_PROFILE_CFG  "/profile.cfg"
#define FILENAME_ADMIN_CSS    "/admin.css"

// Administration URLs
// They don't begin with / and are prefixed by adminPrefix configured in system.cfg
#define URL_ADMIN_SAVE        "save.htm"
#define URL_ADMIN_RESET       "reset.htm"
#define URL_ADMIN_CSS         "admin.css"

// HTML markup used by configuration UI
#define ADMIN_HTML_HEADER     "<html><head><title>WifiBeaconESP Administration</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /><link rel=\"stylesheet\" href=\"admin.css\" /></head><body><h1>WifiBeaconESP Administration</h1>\n"
#define ADMIN_HTML_FOOTER     "\n<footer><div>Copyright &copy; Michal A. Valasek, 2022</div><div>www.rider.cz | www.altair.blog</div><div>github.com/ridercz/WifiBeaconESP</div></footer></body></html>"

// Defaults for system.cfg
#define DEFAULT_PROFILE_NAME  "_DEFAULT"
#define DEFAULT_ADMIN_PREFIX  "/admin/" // has to begin and end with /

// SPIFFS file modes
#define FILE_READ             "r"
#define FILE_APPEND           "a"
#define FILE_WRITE            "w"

// Miscellaneous
#define RESTART_DELAY         5         // s - delay before restart from web admin
#define NORMAL_BLINK_INTERVAL 1000      // ms - blink for normal operation
#define ERROR_BLINK_INTERVAL  100       // ms - blink for error state
#define MDNS_HOST_NAME        "beacon"  // .local domain is assumed
