#pragma once

#include <QMainWindow>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QSet>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QProcess;
class QTableWidgetItem;
class QRadioButton;
class QLabel;
class QSystemTrayIcon;
class QCloseEvent;
class QTabWidget;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void setupSqlTab(class QTabWidget* tabs);
    void setupRunTab(class QTabWidget* tabs);
    void setupIpTab(class QTabWidget* tabs);
    void setupModernUi();
    void setupAutoSave();
    void setupTray();
    void saveSettingsSoon();
    void applyProcessWindowVisibility(qint64 processId, bool visible) const;
    bool setStartWithWindows(bool enabled);
    bool isStartWithWindowsEnabled() const;
    void minimizeToTray();
    bool areAllEnabledProcessesRunning() const;
    void applyTheme(const QString& themeId);
    void applyLanguage(const QString& languageId);
    void toggleLanguage();
    void toggleTheme();
    void updateTopBarButtons();
    void retranslateUi();
    QString l(const QString& ru, const QString& en) const;
    void stopAllRunningProcesses();
    QJsonObject buildCurrentProfileState() const;
    void applyProfileState(const QJsonObject& state);
    void saveCurrentToProfile(const QString& profileId);
    void applyProfile(const QString& profileId);

    void addProcessRow(const QString& name, const QString& relativePath, bool enabled, bool autoRestart, const QString& status, int startOrder, int startDelayMs);
    void loadProcessSettings();
    void saveProcessSettings() const;
    void startAllProcesses();
    void stopAllProcesses();
    void startProcessForRow(int row, bool manualStart);
    void stopProcessForRow(int row);
    void monitorProcesses();
    void addProcessFromDialog();
    void removeSelectedProcess();

    void testSqlConnection();
    void loadOdbcFromConfigs();
    void detectSqlServerAuto();
    void scanSqlConfigFiles();
    void updateSqlPasswordInConfigs();
    void scanIpFiles();
    void applyIpChanges();
    void detectIpAuto();
    QString detectRepositoryRoot() const;
    QStringList enumerateOdbcConfigFiles(const QString& root) const;
    QStringList enumerateConfigFiles(const QString& root) const;
    QStringList enumerateIpConfigFiles(const QString& root) const;
    static bool isTargetIpConfigFile(const QString& path);
    static QString readIniValue(const QString& content, const QString& key);
    static QString getLocalIpAddress();
    static QString getExternalIpAddress();
    QStringList detectSqlServers() const;
    static void addSqlInstancesFromRegistry(const QString& root, QSet<QString>& results);
    static void addInstalledSqlInstances(const QString& root, QSet<QString>& results);
    static void addSqlServices(QSet<QString>& results);
    void addSqlServersFromSqlCmd(QSet<QString>& results) const;
    static QString normalizeSqlServerToken(const QString& value);
    QString resolveSqlCmdPath() const;
    static bool tryReadFile(const QString& path, QString& content, QString& encodingTag, bool& hasUtf8Bom);
    static bool writeFile(const QString& path, const QString& content, const QString& encodingTag, bool hasUtf8Bom);
    static QString makeBackupPath(const QString& path);

    QString settingsPath() const;
    QString keyForRow(int row) const;
    static bool tableCheckState(const QTableWidgetItem* item);
    static QTableWidgetItem* makeCheckItem(bool checked);
    static QTableWidgetItem* makeReadOnlyItem(const QString& text);

private:
    QTabWidget* _tabs = nullptr;
    QPushButton* _languageToggleButton = nullptr;
    QPushButton* _themeToggleButton = nullptr;
    QLineEdit* _sqlServerEdit = nullptr;
    QLineEdit* _sqlDsnEdit = nullptr;
    QLineEdit* _sqlDatabaseEdit = nullptr;
    QLineEdit* _sqlLoginEdit = nullptr;
    QLineEdit* _sqlPasswordEdit = nullptr;
    QCheckBox* _sqlWindowsAuthCheck = nullptr;
    QPushButton* _sqlTestButton = nullptr;
    QPushButton* _sqlApplyOdbcButton = nullptr;
    QPushButton* _sqlDetectButton = nullptr;
    QPushButton* _scanSqlConfigButton = nullptr;
    QPushButton* _updateSqlPasswordButton = nullptr;
    QLineEdit* _oldSqlPasswordEdit = nullptr;
    QLineEdit* _newSqlPasswordEdit = nullptr;
    QCheckBox* _sqlBackupCheck = nullptr;
    QTableWidget* _sqlConfigFilesTable = nullptr;

    QLineEdit* _publicIpEdit = nullptr;
    QLineEdit* _serverRootEdit = nullptr;
    QCheckBox* _skipLocalIpCheck = nullptr;
    QCheckBox* _createBackupCheck = nullptr;
    QPushButton* _scanIpButton = nullptr;
    QPushButton* _applyIpButton = nullptr;
    QPushButton* _detectIpButton = nullptr;
    QLabel* _localIpLabel = nullptr;
    QLabel* _externalIpLabel = nullptr;
    QRadioButton* _useLocalIpRadio = nullptr;
    QRadioButton* _useExternalIpRadio = nullptr;
    QTableWidget* _ipFilesTable = nullptr;
    QString _localIpDetected;
    QString _externalIpDetected;

    QTableWidget* _processesTable = nullptr;
    QPushButton* _addExeButton = nullptr;
    QPushButton* _removeExeButton = nullptr;
    QPushButton* _startAllButton = nullptr;
    QPushButton* _stopAllButton = nullptr;
    QPushButton* _saveProcessesButton = nullptr;
    QCheckBox* _startWithWindowsCheck = nullptr;
    QCheckBox* _minimizeToTrayAfterStartCheck = nullptr;
    QCheckBox* _closeToTrayOnCloseCheck = nullptr;
    QCheckBox* _stopProcessesOnCloseCheck = nullptr;
    QComboBox* _profileCombo = nullptr;
    QPushButton* _applyProfileButton = nullptr;
    QPushButton* _saveProfileButton = nullptr;
    QSystemTrayIcon* _trayIcon = nullptr;
    QAction* _trayRestoreAction = nullptr;
    QAction* _trayExitAction = nullptr;

    QTimer* _monitorTimer = nullptr;
    QTimer* _autoSaveTimer = nullptr;
    QHash<QString, QProcess*> _runningProcesses;
    QHash<QString, QDateTime> _lastStartAttempts;
    QHash<QString, int> _restartAttemptsInWindow;
    QHash<QString, QDateTime> _restartWindowStart;
    QHash<QString, QDateTime> _restartBlockedUntil;
    QJsonObject _profiles;
    QString _repoRoot;
    mutable QString _sqlCmdPath;
    bool _isLoadingSettings = false;
    QString _currentLanguage = "ru";
    QString _currentTheme = "dark";
    bool _closeBehaviorPromptShown = false;
    int _ipTabIndex = -1;
    int _sqlTabIndex = -1;
    int _runTabIndex = -1;
};
