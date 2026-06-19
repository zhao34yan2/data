#ifndef __ANCS_MANAGER_H__
#define __ANCS_MANAGER_H__

#include <string>
#include <functional>
#include <cstdint>
#include <esp_log.h>

// ANCS UUIDs (Apple Notification Center Service)
#define ANCS_SERVICE_UUID        0xD000, 0xD0, 0x4E99, 0xA40F, {0x79, 0x05, 0xF4, 0x31, 0xB5, 0xCE}
#define ANCS_SVC_UUID128         "7905F431-B5CE-4E99-A40F-4B1E122D00D0"
#define ANCS_NS_CHR_UUID128      "9FBF120D-6301-42D9-8C58-25E699A21DBD"  // Notification Source (notify)
#define ANCS_CP_CHR_UUID128      "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9"  // Control Point (write)
#define ANCS_DS_CHR_UUID128      "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB"  // Data Source (notify)

// ANCS Category IDs
enum AncsCategoryId : uint8_t {
    kAncsCategoryOther = 0,
    kAncsCategoryIncomingCall = 1,
    kAncsCategoryMissedCall = 2,
    kAncsCategoryVoicemail = 3,
    kAncsCategorySocial = 4,
    kAncsCategorySchedule = 5,
    kAncsCategoryEmail = 6,
    kAncsCategoryNews = 7,
    kAncsCategoryHealth = 8,
    kAncsCategoryBusiness = 9,
    kAncsCategoryLocation = 10,
    kAncsCategoryEntertainment = 11,
};

// ANCS Event IDs
enum AncsEventId : uint8_t {
    kAncsEventAdded = 0,
    kAncsEventModified = 1,
    kAncsEventRemoved = 2,
};

// ANCS notification callback
struct AncsNotification {
    uint32_t uid;
    uint8_t event_id;
    uint8_t event_flags;
    uint8_t category_id;
    std::string app_id;
    std::string title;
    std::string message;
    std::string date;
    bool is_call;
    bool is_important;

    AncsNotification() : uid(0), event_id(0), event_flags(0), category_id(0),
                         is_call(false), is_important(false) {}
};

class AncsManager {
public:
    using NotificationCallback = std::function<void(const AncsNotification&)>;

    static AncsManager& GetInstance();

    bool Start();
    void Stop();
    bool IsConnected() const { return connected_; }
    void SetNotificationCallback(NotificationCallback cb) { callback_ = cb; }

private:
    AncsManager();
    ~AncsManager();

    // NimBLE GAP event handlers
    static void OnSync(void);
    static void OnReset(int reason);
    static void OnBleConnect(struct ble_gap_event *event, void *arg);
    static int GapCallback(struct ble_gap_event *event, void *arg);

    // GATT operations
    static int OnServiceDiscovery(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   const struct ble_gatt_svc *service, void *arg);
    static int OnCharDiscovery(uint16_t conn_handle, const struct ble_gatt_error *error,
                                const struct ble_gatt_chr *chr, void *arg);
    static int OnNsSubscribe(uint16_t conn_handle, const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attr, void *arg);
    static int OnDsSubscribe(uint16_t conn_handle, const struct ble_gatt_error *error,
                              struct ble_gatt_attr *attr, void *arg);
    static int OnNsNotification(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg);
    static int OnDsNotification(uint16_t conn_handle, const struct ble_gatt_error *error,
                                 struct ble_gatt_attr *attr, void *arg);
    static int OnCpWrite(uint16_t conn_handle, const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr, void *arg);

    // Internal methods
    void StartScanning();
    void StopScanning();
    void ConnectToDevice(const void *addr_ptr);
    void DiscoverAncsServices();
    static void RequestNotificationAttributes(uint32_t uid);
    bool ParseNsPacket(const uint8_t *data, uint16_t len);
    void ProcessDsData(const uint8_t *data, uint16_t len);

    // NimBLE host task
    static void NimbleHostTask(void *param);

    bool initialized_ = false;
    bool connected_ = false;
    uint16_t conn_handle_ = 0;

    // ANCS characteristic handles
    uint16_t ns_handle_ = 0;   // Notification Source
    uint16_t ns_ccc_handle_ = 0;
    uint16_t cp_handle_ = 0;   // Control Point
    uint16_t ds_handle_ = 0;   // Data Source
    uint16_t ds_ccc_handle_ = 0;

    // Current notification being processed
    AncsNotification current_notification_;
    bool waiting_for_attributes_ = false;
    int ds_attr_bytes_remaining_ = 0;
    int ds_attr_state_ = 0;  // 0=waiting cmd, 1=waiting uid, 2=reading attrs

    NotificationCallback callback_;

    static const char *TAG;
    static const int ANCS_SCAN_DURATION_SECONDS = 30;
};

#endif // __ANCS_MANAGER_H__