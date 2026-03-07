#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QPainter>
#include <QLinearGradient>
#include <QHostAddress>
#include <QHostInfo>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStatusBar>
#include <QSet>
#include <QSettings>
#include <QTableWidget>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QAbstractItemView>
#include <QStringConverter>
#include <QStyleFactory>
#include <QSignalBlocker>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QStyle>
#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef Q_OS_WIN
namespace
{
QIcon createMuThemeIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 0, 64);
    bg.setColorAt(0.0, QColor(90, 18, 20));
    bg.setColorAt(1.0, QColor(20, 11, 15));
    painter.setBrush(bg);
    painter.setPen(QPen(QColor(180, 48, 48), 2));
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 10, 10);

    QLinearGradient fg(0, 10, 0, 54);
    fg.setColorAt(0.0, QColor(255, 224, 128));
    fg.setColorAt(1.0, QColor(196, 127, 44));
    painter.setPen(QPen(QBrush(fg), 4));
    painter.setFont(QFont("Segoe UI", 24, QFont::Black));
    painter.drawText(QRect(0, 8, 64, 48), Qt::AlignCenter, "MU");

    painter.setPen(QPen(QColor(255, 197, 120), 1));
    painter.drawLine(12, 51, 52, 51);
    return QIcon(pixmap);
}

struct WindowVisibilityContext
{
    DWORD processId = 0;
    bool visible = false;
};

BOOL CALLBACK ToggleWindowVisibilityProc(HWND hwnd, LPARAM lParam)
{
    auto* context = reinterpret_cast<WindowVisibilityContext*>(lParam);
    if (!context)
    {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != context->processId)
    {
        return TRUE;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return TRUE;
    }

    ShowWindow(hwnd, context->visible ? SW_SHOW : SW_HIDE);
    return TRUE;
}
}
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    _repoRoot = detectRepositoryRoot();
    setupUi();
    _isLoadingSettings = true;
    loadProcessSettings();
    _isLoadingSettings = false;

    _monitorTimer = new QTimer(this);
    _monitorTimer->setInterval(3000);
    connect(_monitorTimer, &QTimer::timeout, this, &MainWindow::monitorProcesses);
    _monitorTimer->start();
    monitorProcesses();
    saveSettingsSoon();
}

void MainWindow::setupUi()
{
    setWindowIcon(createMuThemeIcon());
    resize(760, 470);
    setMinimumSize(680, 420);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    auto* topBar = new QWidget(root);
    auto* topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(6);
    _languageToggleButton = new QPushButton(topBar);
    _languageToggleButton->setObjectName("langToggleButton");
    _languageToggleButton->setFixedWidth(30);
    _themeToggleButton = new QPushButton(topBar);
    _themeToggleButton->setObjectName("themeToggleButton");
    _themeToggleButton->setFixedWidth(30);
    topBarLayout->addStretch();
    topBarLayout->addWidget(_languageToggleButton);
    topBarLayout->addWidget(_themeToggleButton);
    rootLayout->addWidget(topBar);

    _tabs = new QTabWidget(root);
    rootLayout->addWidget(_tabs);

    setupIpTab(_tabs);
    setupSqlTab(_tabs);
    setupRunTab(_tabs);
    setupModernUi();
    setupAutoSave();
    setupTray();

    connect(_languageToggleButton, &QPushButton::clicked, this, &MainWindow::toggleLanguage);
    connect(_themeToggleButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    applyLanguage("ru");
    applyTheme("dark");

    setCentralWidget(root);
    statusBar()->showMessage(l("Готово", "Ready"));
}

void MainWindow::setupModernUi()
{
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    const auto tables = findChildren<QTableWidget*>();
    for (auto* table : tables)
    {
        table->setAlternatingRowColors(true);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setHighlightSections(false);
        table->verticalHeader()->setDefaultSectionSize(22);
    }
}

QString MainWindow::l(const QString& ru, const QString& en) const
{
    return _currentLanguage == "en" ? en : ru;
}

void MainWindow::applyTheme(const QString& themeId)
{
    _currentTheme = (themeId == "light") ? "light" : "dark";

    if (_currentTheme == "light")
    {
        setStyleSheet(R"(
            QMainWindow { background: #F3F4F6; color: #111827; }
            QGroupBox { border: 1px solid #D1D5DB; border-radius: 8px; margin-top: 7px; padding-top: 6px; }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #1F2937; }
            QLabel { color: #1F2937; }
            QLineEdit, QComboBox { background: #FFFFFF; color: #111827; border: 1px solid #CBD5E1; border-radius: 6px; padding: 3px 7px; min-height: 18px; }
            QPushButton { background: #2563EB; color: white; border: none; border-radius: 6px; padding: 4px 8px; min-height: 20px; }
            QPushButton#langToggleButton, QPushButton#themeToggleButton { background: #E5E7EB; color: #111827; border: 1px solid #CBD5E1; font-size: 14px; padding: 0; }
            QPushButton:hover { background: #3B82F6; }
            QPushButton:pressed { background: #1D4ED8; }
            QCheckBox, QRadioButton { color: #111827; spacing: 6px; }
            QTableWidget { background: #FFFFFF; alternate-background-color: #F9FAFB; border: 1px solid #CBD5E1; gridline-color: #E5E7EB; color: #111827; }
            QHeaderView::section { background: #E5E7EB; color: #111827; border: none; border-right: 1px solid #D1D5DB; padding: 6px; }
            QTabWidget::pane { border: 1px solid #D1D5DB; border-radius: 8px; top: -1px; }
            QTabBar::tab { background: #E5E7EB; color: #111827; border: 1px solid #D1D5DB; padding: 6px 10px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 3px; }
            QTabBar::tab:selected { background: #2563EB; color: #FFFFFF; }
            QStatusBar { background: #FFFFFF; color: #111827; border-top: 1px solid #D1D5DB; }
        )");
        updateTopBarButtons();
        return;
    }

    setStyleSheet(R"(
        QMainWindow { background: #111827; color: #E5E7EB; }
        QGroupBox { border: 1px solid #374151; border-radius: 8px; margin-top: 7px; padding-top: 6px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #D1D5DB; }
        QLabel { color: #D1D5DB; }
        QLineEdit, QComboBox { background: #0F172A; color: #F9FAFB; border: 1px solid #334155; border-radius: 6px; padding: 3px 7px; min-height: 18px; }
        QPushButton { background: #2563EB; color: white; border: none; border-radius: 6px; padding: 4px 8px; min-height: 20px; }
        QPushButton#langToggleButton, QPushButton#themeToggleButton { background: #1F2937; color: #E5E7EB; border: 1px solid #374151; font-size: 14px; padding: 0; }
        QPushButton:hover { background: #3B82F6; }
        QPushButton:pressed { background: #1D4ED8; }
        QCheckBox, QRadioButton { color: #E5E7EB; spacing: 6px; }
        QTableWidget { background: #0B1220; alternate-background-color: #111827; border: 1px solid #334155; gridline-color: #1F2937; color: #E5E7EB; }
        QHeaderView::section { background: #1F2937; color: #D1D5DB; border: none; border-right: 1px solid #374151; padding: 6px; }
        QTabWidget::pane { border: 1px solid #374151; border-radius: 8px; top: -1px; }
        QTabBar::tab { background: #1F2937; color: #D1D5DB; border: 1px solid #374151; padding: 6px 10px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 3px; }
        QTabBar::tab:selected { background: #2563EB; color: #FFFFFF; }
        QStatusBar { background: #0B1220; color: #D1D5DB; border-top: 1px solid #374151; }
    )");
    updateTopBarButtons();
}

void MainWindow::applyLanguage(const QString& languageId)
{
    _currentLanguage = (languageId == "en") ? "en" : "ru";
    updateTopBarButtons();
    retranslateUi();
}

void MainWindow::toggleLanguage()
{
    applyLanguage(_currentLanguage == "en" ? "ru" : "en");
    saveSettingsSoon();
}

void MainWindow::toggleTheme()
{
    applyTheme(_currentTheme == "light" ? "dark" : "light");
    saveSettingsSoon();
}

void MainWindow::updateTopBarButtons()
{
    if (_languageToggleButton)
    {
        _languageToggleButton->setText(_currentLanguage == "en" ? "🇺🇸" : "🇷🇺");
        _languageToggleButton->setToolTip(l("Сменить язык", "Switch language"));
    }
    if (_themeToggleButton)
    {
        _themeToggleButton->setText(_currentTheme == "light" ? "☀" : "🌙");
        _themeToggleButton->setToolTip(l("Сменить тему", "Switch theme"));
    }
}

void MainWindow::retranslateUi()
{
    setWindowTitle(l("MU Online Сервер-менеджер", "MU Online Server Manager"));
    if (_tabs)
    {
        if (_ipTabIndex >= 0) _tabs->setTabText(_ipTabIndex, l("Публичный IP", "Public IP"));
        if (_sqlTabIndex >= 0) _tabs->setTabText(_sqlTabIndex, l("Config SQL", "SQL Config"));
        if (_runTabIndex >= 0) _tabs->setTabText(_runTabIndex, l("Автозапуск", "Autostart"));
    }
    updateTopBarButtons();
    if (auto* ipTopBox = findChild<QGroupBox*>("ipTopBox")) ipTopBox->setTitle(l("Публичный IP", "Public IP"));
    if (auto* ipTableBox = findChild<QGroupBox*>("ipTableBox")) ipTableBox->setTitle(l("Найденные файлы с IP", "Detected IP files"));
    if (auto* sqlConnBox = findChild<QGroupBox*>("sqlConnBox")) sqlConnBox->setTitle(l("SQL подключение", "SQL connection"));
    if (auto* sqlConfigBox = findChild<QGroupBox*>("sqlConfigBox")) sqlConfigBox->setTitle(l("SQL конфиги", "SQL configs"));
    if (auto* runMonitorBox = findChild<QGroupBox*>("runMonitorBox")) runMonitorBox->setTitle(l("Мониторинг", "Monitoring"));
    if (auto* profileLabel = findChild<QLabel*>("profileLabel")) profileLabel->setText(l("Профиль", "Profile"));
    if (auto* runMonitorHint = findChild<QLabel*>("runMonitorHint")) runMonitorHint->setText(l("Мониторинг процессов и автоперезапуск при падении.", "Process monitoring and auto-restart on crash."));
    if (auto* ipPublicLabel = findChild<QLabel*>("ipPublicLabel")) ipPublicLabel->setText(l("Публичный IP", "Public IP"));
    if (auto* ipRootLabel = findChild<QLabel*>("ipRootLabel")) ipRootLabel->setText(l("Корень сервера", "Server root"));
    if (auto* ipLocalCaption = findChild<QLabel*>("ipLocalCaption")) ipLocalCaption->setText(l("Локальный:", "Local:"));
    if (auto* ipExternalCaption = findChild<QLabel*>("ipExternalCaption")) ipExternalCaption->setText(l("Внешний:", "External:"));
    if (_useLocalIpRadio) _useLocalIpRadio->setText(l("Локальный", "Local"));
    if (_useExternalIpRadio) _useExternalIpRadio->setText(l("Внешний", "External"));
    if (_skipLocalIpCheck) _skipLocalIpCheck->setText(l("Не менять 127.0.0.1 и 0.0.0.0", "Do not change 127.0.0.1 and 0.0.0.0"));
    if (_createBackupCheck) _createBackupCheck->setText(l("Создавать backup перед заменой", "Create backup before replace"));
    if (_sqlWindowsAuthCheck) _sqlWindowsAuthCheck->setText(l("Windows-аутентификация", "Windows authentication"));
    if (_sqlBackupCheck) _sqlBackupCheck->setText(l("Резервная копия перед изменением", "Backup before changes"));
    if (auto* sqlServerLabel = findChild<QLabel*>("sqlServerLabel")) sqlServerLabel->setText(l("SQL Сервер", "SQL Server"));
    if (auto* sqlDsnLabel = findChild<QLabel*>("sqlDsnLabel")) sqlDsnLabel->setText("DSN");
    if (auto* sqlDbLabel = findChild<QLabel*>("sqlDbLabel")) sqlDbLabel->setText(l("База", "Database"));
    if (auto* sqlLoginLabel = findChild<QLabel*>("sqlLoginLabel")) sqlLoginLabel->setText(l("Логин", "Login"));
    if (auto* sqlPasswordLabel = findChild<QLabel*>("sqlPasswordLabel")) sqlPasswordLabel->setText(l("Пароль", "Password"));
    if (auto* sqlOldPasswordLabel = findChild<QLabel*>("sqlOldPasswordLabel")) sqlOldPasswordLabel->setText(l("Старый пароль", "Old password"));
    if (auto* sqlNewPasswordLabel = findChild<QLabel*>("sqlNewPasswordLabel")) sqlNewPasswordLabel->setText(l("Новый пароль", "New password"));
    if (_ipFilesTable) _ipFilesTable->setHorizontalHeaderLabels({l("Обновить", "Apply"), l("Совпадений", "Matches"), l("Файл", "File"), l("Пример IP", "IP sample")});
    if (_sqlConfigFilesTable) _sqlConfigFilesTable->setHorizontalHeaderLabels({l("Обновить", "Apply"), l("Совпадений", "Matches"), l("Файл", "File"), l("Пример", "Sample")});
    if (_processesTable) _processesTable->setHorizontalHeaderLabels({l("Вкл", "On"), l("Рестарт", "Restart"), l("Имя", "Name"), l("Путь EXE", "EXE path"), l("Статус", "Status"), l("Окно", "Window"), l("Порядок", "Order"), l("Задержка мс", "Delay ms")});

    if (_addExeButton) _addExeButton->setToolTip(l("Добавить EXE", "Add EXE"));
    if (_removeExeButton) _removeExeButton->setToolTip(l("Удалить выбранный процесс", "Remove selected process"));
    if (_startAllButton) _startAllButton->setToolTip(l("Запустить все включенные процессы", "Start all enabled processes"));
    if (_stopAllButton) _stopAllButton->setToolTip(l("Остановить все процессы", "Stop all processes"));
    if (_saveProcessesButton) _saveProcessesButton->setToolTip(l("Сохранить список процессов", "Save process list"));
    if (_scanIpButton) _scanIpButton->setToolTip(l("Сканировать IP файлы", "Scan IP files"));
    if (_applyIpButton) _applyIpButton->setToolTip(l("Применить IP во всех выбранных файлах", "Apply IP to selected files"));
    if (_detectIpButton) _detectIpButton->setToolTip(l("Автоопределение локального/внешнего IP", "Auto detect local/external IP"));
    if (_sqlDetectButton) _sqlDetectButton->setToolTip(l("Автоопределение SQL Server", "Auto detect SQL Server"));
    if (_sqlApplyOdbcButton) _sqlApplyOdbcButton->setToolTip(l("Загрузить ODBC из конфигов", "Load ODBC from configs"));
    if (_sqlTestButton) _sqlTestButton->setToolTip(l("Проверить ODBC подключение", "Test ODBC connection"));
    if (_scanSqlConfigButton) _scanSqlConfigButton->setToolTip(l("Сканировать SQL конфиги", "Scan SQL configs"));
    if (_updateSqlPasswordButton) _updateSqlPasswordButton->setToolTip(l("Обновить SQL пароль в выбранных файлах", "Update SQL password in selected files"));
    if (_startWithWindowsCheck) _startWithWindowsCheck->setText(l("Запускать утилиту при старте Windows", "Start utility with Windows"));
    if (_minimizeToTrayAfterStartCheck) _minimizeToTrayAfterStartCheck->setText(l("После старта всех сервисов свернуть в трей", "Minimize to tray after all services start"));
    if (_closeToTrayOnCloseCheck) _closeToTrayOnCloseCheck->setText(l("При нажатии X сворачивать в трей", "Minimize to tray on X button"));
    if (_stopProcessesOnCloseCheck) _stopProcessesOnCloseCheck->setText(l("При закрытии завершать все процессы", "Stop all processes on app close"));
    if (_profileCombo)
    {
        _profileCombo->setItemText(0, l("Локальный", "Local"));
        _profileCombo->setItemText(1, l("Тестовый", "Test"));
        _profileCombo->setItemText(2, l("Продакшн", "Prod"));
        _profileCombo->setToolTip(l("Профиль окружения", "Environment profile"));
    }
    if (_applyProfileButton) _applyProfileButton->setToolTip(l("Применить профиль", "Apply profile"));
    if (_saveProfileButton) _saveProfileButton->setToolTip(l("Сохранить в профиль", "Save to profile"));
    if (_trayIcon) _trayIcon->setToolTip(l("MU Online Server Manager", "MU Online Server Manager"));
    if (_trayRestoreAction) _trayRestoreAction->setText(l("Показать", "Show"));
    if (_trayExitAction) _trayExitAction->setText(l("Выход", "Exit"));
}

void MainWindow::setupAutoSave()
{
    _autoSaveTimer = new QTimer(this);
    _autoSaveTimer->setSingleShot(true);
    _autoSaveTimer->setInterval(700);
    connect(_autoSaveTimer, &QTimer::timeout, this, [this]()
    {
        saveProcessSettings();
    });

    auto queueAutoSave = [this]()
    {
        saveSettingsSoon();
    };

    connect(_publicIpEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_serverRootEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_skipLocalIpCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_createBackupCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_sqlServerEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_sqlDsnEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_sqlDatabaseEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_sqlLoginEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_sqlPasswordEdit, &QLineEdit::textChanged, this, [queueAutoSave](const QString&) { queueAutoSave(); });
    connect(_sqlWindowsAuthCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_sqlBackupCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_startWithWindowsCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_minimizeToTrayAfterStartCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_closeToTrayOnCloseCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
    connect(_stopProcessesOnCloseCheck, &QCheckBox::toggled, this, [queueAutoSave](bool) { queueAutoSave(); });
}

void MainWindow::saveSettingsSoon()
{
    if (_isLoadingSettings || !_autoSaveTimer)
    {
        return;
    }
    _autoSaveTimer->start();
}

bool MainWindow::setStartWithWindows(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings runReg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    const QString valueName = "ServerQuickManagerQt";
    if (enabled)
    {
        const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        runReg.setValue(valueName, QString("\"%1\"").arg(exePath));
    }
    else
    {
        runReg.remove(valueName);
    }
    runReg.sync();
    return runReg.status() == QSettings::NoError;
#else
    Q_UNUSED(enabled);
    return true;
#endif
}

bool MainWindow::isStartWithWindowsEnabled() const
{
#ifdef Q_OS_WIN
    QSettings runReg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return !runReg.value("ServerQuickManagerQt").toString().trimmed().isEmpty();
#else
    return false;
#endif
}

void MainWindow::minimizeToTray()
{
    if (!_trayIcon)
    {
        return;
    }
    hide();
    _trayIcon->showMessage("MU Online Server Manager", l("Все сервисы запущены. Окно свернуто в трей.", "All services are started. Window is minimized to tray."), QSystemTrayIcon::Information, 2000);
}

bool MainWindow::areAllEnabledProcessesRunning() const
{
    bool hasEnabled = false;
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        if (!tableCheckState(_processesTable->item(row, 0)))
        {
            continue;
        }
        hasEnabled = true;
        const QString key = keyForRow(row);
        auto* process = _runningProcesses.value(key, nullptr);
        if (!process || process->state() == QProcess::NotRunning)
        {
            return false;
        }
    }
    return hasEnabled;
}

void MainWindow::stopAllRunningProcesses()
{
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        stopProcessForRow(row);
    }
}

QJsonObject MainWindow::buildCurrentProfileState() const
{
    QJsonObject state;
    state["serverRoot"] = _repoRoot;
    state["lastPublicIp"] = _publicIpEdit ? _publicIpEdit->text().trimmed() : QString();
    state["sqlServer"] = _sqlServerEdit ? _sqlServerEdit->text().trimmed() : QString();
    state["sqlDsn"] = _sqlDsnEdit ? _sqlDsnEdit->text().trimmed() : QString();
    state["sqlDatabase"] = _sqlDatabaseEdit ? _sqlDatabaseEdit->text().trimmed() : QString();
    state["sqlLogin"] = _sqlLoginEdit ? _sqlLoginEdit->text().trimmed() : QString();
    state["sqlPassword"] = _sqlPasswordEdit ? _sqlPasswordEdit->text() : QString();
    state["sqlWindowsAuth"] = _sqlWindowsAuthCheck ? _sqlWindowsAuthCheck->isChecked() : false;
    state["skipLocalIp"] = _skipLocalIpCheck ? _skipLocalIpCheck->isChecked() : true;
    state["createIpBackup"] = _createBackupCheck ? _createBackupCheck->isChecked() : true;
    state["createSqlBackup"] = _sqlBackupCheck ? _sqlBackupCheck->isChecked() : true;
    state["startWithWindows"] = _startWithWindowsCheck ? _startWithWindowsCheck->isChecked() : false;
    state["minimizeToTrayAfterStart"] = _minimizeToTrayAfterStartCheck ? _minimizeToTrayAfterStartCheck->isChecked() : false;
    state["closeToTrayOnClose"] = _closeToTrayOnCloseCheck ? _closeToTrayOnCloseCheck->isChecked() : false;
    state["stopProcessesOnClose"] = _stopProcessesOnCloseCheck ? _stopProcessesOnCloseCheck->isChecked() : false;

    QJsonArray processes;
    if (_processesTable)
    {
        for (int row = 0; row < _processesTable->rowCount(); ++row)
        {
            QJsonObject obj;
            obj["name"] = _processesTable->item(row, 2)->text();
            obj["relativePath"] = _processesTable->item(row, 3)->text();
            obj["isEnabled"] = tableCheckState(_processesTable->item(row, 0));
            obj["autoRestart"] = tableCheckState(_processesTable->item(row, 1));
            obj["showWindow"] = tableCheckState(_processesTable->item(row, 5));
            obj["startOrder"] = _processesTable->item(row, 6)->text().toInt();
            obj["startDelayMs"] = _processesTable->item(row, 7)->text().toInt();
            processes.append(obj);
        }
    }
    state["processes"] = processes;
    return state;
}

void MainWindow::applyProfileState(const QJsonObject& state)
{
    _isLoadingSettings = true;
    if (_publicIpEdit) _publicIpEdit->setText(state.value("lastPublicIp").toString());
    if (_sqlServerEdit) _sqlServerEdit->setText(state.value("sqlServer").toString(".\\SQLEXPRESS"));
    if (_sqlDsnEdit) _sqlDsnEdit->setText(state.value("sqlDsn").toString("MuOnline"));
    if (_sqlDatabaseEdit) _sqlDatabaseEdit->setText(state.value("sqlDatabase").toString("MuOnline"));
    if (_sqlLoginEdit) _sqlLoginEdit->setText(state.value("sqlLogin").toString("sa"));
    if (_sqlPasswordEdit) _sqlPasswordEdit->setText(state.value("sqlPassword").toString());
    if (_sqlWindowsAuthCheck) _sqlWindowsAuthCheck->setChecked(state.value("sqlWindowsAuth").toBool(false));
    if (_skipLocalIpCheck) _skipLocalIpCheck->setChecked(state.value("skipLocalIp").toBool(true));
    if (_createBackupCheck) _createBackupCheck->setChecked(state.value("createIpBackup").toBool(true));
    if (_sqlBackupCheck) _sqlBackupCheck->setChecked(state.value("createSqlBackup").toBool(true));
    if (_minimizeToTrayAfterStartCheck) _minimizeToTrayAfterStartCheck->setChecked(state.value("minimizeToTrayAfterStart").toBool(false));
    if (_closeToTrayOnCloseCheck) _closeToTrayOnCloseCheck->setChecked(state.value("closeToTrayOnClose").toBool(false));
    if (_stopProcessesOnCloseCheck) _stopProcessesOnCloseCheck->setChecked(state.value("stopProcessesOnClose").toBool(false));

    const QString serverRoot = state.value("serverRoot").toString();
    if (!serverRoot.isEmpty() && QDir(serverRoot).exists())
    {
        _repoRoot = serverRoot;
        if (_serverRootEdit) _serverRootEdit->setText(_repoRoot);
    }
    _processesTable->setRowCount(0);
    const auto arr = state.value("processes").toArray();
    for (const auto& value : arr)
    {
        const auto obj = value.toObject();
        addProcessRow(
            obj.value("name").toString(),
            obj.value("relativePath").toString(),
            obj.value("isEnabled").toBool(true),
            obj.value("autoRestart").toBool(true),
            l("Остановлен", "Stopped"),
            obj.value("startOrder").toInt(_processesTable->rowCount() + 1),
            obj.value("startDelayMs").toInt(400));
        const int row = _processesTable->rowCount() - 1;
        _processesTable->item(row, 5)->setCheckState(obj.value("showWindow").toBool(false) ? Qt::Checked : Qt::Unchecked);
    }
    _isLoadingSettings = false;
}

void MainWindow::saveCurrentToProfile(const QString& profileId)
{
    if (profileId.isEmpty())
    {
        return;
    }
    _profiles.insert(profileId, buildCurrentProfileState());
    saveProcessSettings();
    statusBar()->showMessage(QString("%1: %2").arg(l("Профиль сохранён", "Profile saved"), profileId));
}

void MainWindow::applyProfile(const QString& profileId)
{
    if (profileId.isEmpty())
    {
        return;
    }
    const auto state = _profiles.value(profileId).toObject();
    if (state.isEmpty())
    {
        statusBar()->showMessage(QString("%1: %2").arg(l("Профиль пуст", "Profile is empty"), profileId));
        return;
    }
    applyProfileState(state);
    saveProcessSettings();
    statusBar()->showMessage(QString("%1: %2").arg(l("Профиль применён", "Profile applied"), profileId));
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        return;
    }

    _trayIcon = new QSystemTrayIcon(this);
    QIcon icon = windowIcon();
    if (icon.isNull())
    {
        icon = style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    _trayIcon->setIcon(icon);
    _trayIcon->setToolTip(l("MU Online Server Manager", "MU Online Server Manager"));

    auto* trayMenu = new QMenu(this);
    _trayRestoreAction = trayMenu->addAction(l("Показать", "Show"));
    _trayExitAction = trayMenu->addAction(l("Выход", "Exit"));
    connect(_trayRestoreAction, &QAction::triggered, this, [this]()
    {
        showNormal();
        raise();
        activateWindow();
    });
    connect(_trayExitAction, &QAction::triggered, this, [this]()
    {
        if (_trayIcon)
        {
            _trayIcon->hide();
        }
        if (_stopProcessesOnCloseCheck && _stopProcessesOnCloseCheck->isChecked())
        {
            stopAllRunningProcesses();
        }
        qApp->quit();
    });
    _trayIcon->setContextMenu(trayMenu);
    connect(_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger)
        {
            showNormal();
            raise();
            activateWindow();
        }
    });
    _trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (_trayIcon && !_closeBehaviorPromptShown)
    {
        QMessageBox box(this);
        box.setWindowTitle(l("Поведение при закрытии", "Close behavior"));
        box.setText(l("Что делать при нажатии кнопки X?", "What should happen when clicking X button?"));
        auto* toTrayButton = box.addButton(l("Свернуть в трей", "Minimize to tray"), QMessageBox::AcceptRole);
        auto* closeButton = box.addButton(l("Закрыть приложение", "Close application"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        auto* remember = new QCheckBox(l("Запомнить мой выбор", "Remember my choice"), &box);
        box.setCheckBox(remember);
        box.exec();
        if (box.clickedButton() == toTrayButton)
        {
            _closeToTrayOnCloseCheck->setChecked(true);
            if (remember->isChecked())
            {
                _closeBehaviorPromptShown = true;
                saveSettingsSoon();
            }
            hide();
            event->ignore();
            return;
        }
        if (box.clickedButton() == closeButton)
        {
            _closeToTrayOnCloseCheck->setChecked(false);
            if (remember->isChecked())
            {
                _closeBehaviorPromptShown = true;
                saveSettingsSoon();
            }
        }
        else
        {
            event->ignore();
            return;
        }
    }

    if (_trayIcon && _closeToTrayOnCloseCheck && _closeToTrayOnCloseCheck->isChecked())
    {
        hide();
        event->ignore();
        return;
    }

    if (_trayIcon)
    {
        _trayIcon->hide();
    }
    if (_stopProcessesOnCloseCheck && _stopProcessesOnCloseCheck->isChecked())
    {
        stopAllRunningProcesses();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::setupIpTab(QTabWidget* tabs)
{
    auto* ipTab = new QWidget(tabs);
    auto* ipGrid = new QGridLayout(ipTab);
    ipGrid->setContentsMargins(2, 2, 2, 2);
    ipGrid->setVerticalSpacing(6);
    ipGrid->setHorizontalSpacing(6);
    ipGrid->setColumnStretch(4, 1);
    ipGrid->setRowStretch(2, 1);

    auto* ipTopBox = new QGroupBox(l("Публичный IP", "Public IP"), ipTab);
    ipTopBox->setObjectName("ipTopBox");
    auto* ipTopGrid = new QGridLayout(ipTopBox);
    ipTopGrid->setHorizontalSpacing(4);
    ipTopGrid->setVerticalSpacing(4);
    auto* ipPublicLabel = new QLabel(l("Публичный IP", "Public IP"));
    ipPublicLabel->setObjectName("ipPublicLabel");
    ipTopGrid->addWidget(ipPublicLabel, 0, 0);
    _publicIpEdit = new QLineEdit(ipTopBox);
    _publicIpEdit->setMaximumWidth(170);
    ipTopGrid->addWidget(_publicIpEdit, 0, 1);
    auto* ipRootLabel = new QLabel(l("Корень сервера", "Server root"));
    ipRootLabel->setObjectName("ipRootLabel");
    ipTopGrid->addWidget(ipRootLabel, 0, 2);
    _serverRootEdit = new QLineEdit(_repoRoot, ipTopBox);
    _serverRootEdit->setReadOnly(true);
    _serverRootEdit->setMaximumWidth(280);
    ipTopGrid->addWidget(_serverRootEdit, 0, 3);
    auto* pickRootButton = new QPushButton(ipTopBox);
    pickRootButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    pickRootButton->setToolTip(l("Выбрать корень сервера", "Select server root"));
    pickRootButton->setFixedWidth(30);
    connect(pickRootButton, &QPushButton::clicked, this, [this]()
    {
        const QString selected = QFileDialog::getExistingDirectory(this, l("Выберите корень сервера", "Select server root"), _repoRoot);
        if (selected.isEmpty())
        {
            return;
        }
        _repoRoot = QDir::fromNativeSeparators(selected);
        _serverRootEdit->setText(_repoRoot);
        statusBar()->showMessage(QString("%1: %2").arg(l("Корень сервера", "Server root"), _repoRoot));
    });
    ipTopGrid->addWidget(pickRootButton, 0, 4);
    _scanIpButton = new QPushButton(ipTopBox);
    _scanIpButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    _scanIpButton->setToolTip(l("Сканировать IP файлы", "Scan IP files"));
    _scanIpButton->setFixedWidth(30);
    connect(_scanIpButton, &QPushButton::clicked, this, &MainWindow::scanIpFiles);
    ipTopGrid->addWidget(_scanIpButton, 0, 5);
    ipGrid->addWidget(ipTopBox, 0, 0, 1, 6);

    auto* ipActions = new QWidget(ipTab);
    auto* ipActionsLayout = new QHBoxLayout(ipActions);
    ipActionsLayout->setContentsMargins(0, 0, 0, 0);
    ipActionsLayout->setSpacing(8);
    _skipLocalIpCheck = new QCheckBox(l("Не менять 127.0.0.1 и 0.0.0.0", "Do not change 127.0.0.1 and 0.0.0.0"), ipActions);
    _skipLocalIpCheck->setChecked(true);
    ipActionsLayout->addWidget(_skipLocalIpCheck);
    _createBackupCheck = new QCheckBox(l("Создавать backup перед заменой", "Create backup before replace"), ipActions);
    _createBackupCheck->setChecked(true);
    ipActionsLayout->addWidget(_createBackupCheck);
    _applyIpButton = new QPushButton(ipActions);
    _applyIpButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    _applyIpButton->setToolTip(l("Применить IP во всех выбранных файлах", "Apply IP to selected files"));
    _applyIpButton->setFixedWidth(30);
    connect(_applyIpButton, &QPushButton::clicked, this, &MainWindow::applyIpChanges);
    ipActionsLayout->addWidget(_applyIpButton);
    ipActionsLayout->addStretch();
    ipGrid->addWidget(ipActions, 1, 0, 1, 6);

    auto* detectIpRow = new QWidget(ipTab);
    auto* detectIpLayout = new QHBoxLayout(detectIpRow);
    detectIpLayout->setContentsMargins(0, 0, 0, 0);
    detectIpLayout->setSpacing(8);
    _detectIpButton = new QPushButton(detectIpRow);
    _detectIpButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    _detectIpButton->setToolTip(l("Автоопределение локального/внешнего IP", "Auto detect local/external IP"));
    _detectIpButton->setFixedWidth(30);
    connect(_detectIpButton, &QPushButton::clicked, this, &MainWindow::detectIpAuto);
    detectIpLayout->addWidget(_detectIpButton);
    auto* ipLocalCaption = new QLabel(l("Локальный:", "Local:"), detectIpRow);
    ipLocalCaption->setObjectName("ipLocalCaption");
    detectIpLayout->addWidget(ipLocalCaption);
    _localIpLabel = new QLabel(detectIpRow);
    _localIpLabel->setMinimumWidth(90);
    _localIpLabel->setMaximumWidth(130);
    detectIpLayout->addWidget(_localIpLabel);
    auto* ipExternalCaption = new QLabel(l("Внешний:", "External:"), detectIpRow);
    ipExternalCaption->setObjectName("ipExternalCaption");
    detectIpLayout->addWidget(ipExternalCaption);
    _externalIpLabel = new QLabel(detectIpRow);
    _externalIpLabel->setMinimumWidth(90);
    _externalIpLabel->setMaximumWidth(130);
    detectIpLayout->addWidget(_externalIpLabel);
    _useLocalIpRadio = new QRadioButton(l("Локальный", "Local"), detectIpRow);
    _useExternalIpRadio = new QRadioButton(l("Внешний", "External"), detectIpRow);
    detectIpLayout->addWidget(_useLocalIpRadio);
    detectIpLayout->addWidget(_useExternalIpRadio);
    detectIpLayout->addStretch();
    connect(_useLocalIpRadio, &QRadioButton::toggled, this, [this](bool checked)
    {
        if (checked && !_localIpDetected.isEmpty())
        {
            _publicIpEdit->setText(_localIpDetected);
        }
    });
    connect(_useExternalIpRadio, &QRadioButton::toggled, this, [this](bool checked)
    {
        if (checked && !_externalIpDetected.isEmpty())
        {
            _publicIpEdit->setText(_externalIpDetected);
        }
    });
    ipGrid->addWidget(detectIpRow, 2, 0, 1, 6);

    auto* ipTableBox = new QGroupBox(l("Найденные файлы с IP", "Detected IP files"), ipTab);
    ipTableBox->setObjectName("ipTableBox");
    auto* ipTableLayout = new QVBoxLayout(ipTableBox);
    _ipFilesTable = new QTableWidget(0, 4, ipTableBox);
    _ipFilesTable->setHorizontalHeaderLabels({l("Обновить", "Apply"), l("Совпадений", "Matches"), l("Файл", "File"), l("Пример IP", "IP sample")});
    _ipFilesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    _ipFilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _ipFilesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _ipFilesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    _ipFilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _ipFilesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _ipFilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ipTableLayout->addWidget(_ipFilesTable);
    ipGrid->addWidget(ipTableBox, 3, 0, 1, 6);

    _ipTabIndex = tabs->addTab(ipTab, l("Публичный IP", "Public IP"));
}

void MainWindow::setupSqlTab(QTabWidget* tabs)
{
    auto* sqlTab = new QWidget(tabs);
    auto* sqlLayout = new QVBoxLayout(sqlTab);
    sqlLayout->setContentsMargins(2, 2, 2, 2);
    sqlLayout->setSpacing(6);

    auto* sqlConnBox = new QGroupBox(l("SQL подключение", "SQL connection"), sqlTab);
    sqlConnBox->setObjectName("sqlConnBox");
    auto* sqlConnGrid = new QGridLayout(sqlConnBox);
    sqlConnGrid->setHorizontalSpacing(4);
    sqlConnGrid->setVerticalSpacing(4);

    auto* sqlServerLabel = new QLabel(l("SQL Сервер", "SQL Server"));
    sqlServerLabel->setObjectName("sqlServerLabel");
    sqlConnGrid->addWidget(sqlServerLabel, 0, 0);
    _sqlServerEdit = new QLineEdit(".\\SQLEXPRESS", sqlConnBox);
    _sqlServerEdit->setMaximumWidth(220);
    sqlConnGrid->addWidget(_sqlServerEdit, 0, 1);
    _sqlDetectButton = new QPushButton(sqlConnBox);
    _sqlDetectButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    _sqlDetectButton->setToolTip(l("Автоопределение SQL Server", "Auto detect SQL Server"));
    _sqlDetectButton->setFixedWidth(30);
    sqlConnGrid->addWidget(_sqlDetectButton, 0, 2);
    _sqlApplyOdbcButton = new QPushButton(sqlConnBox);
    _sqlApplyOdbcButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    _sqlApplyOdbcButton->setToolTip(l("Загрузить ODBC из конфигов", "Load ODBC from configs"));
    _sqlApplyOdbcButton->setFixedWidth(30);
    sqlConnGrid->addWidget(_sqlApplyOdbcButton, 0, 3);

    auto* sqlDsnLabel = new QLabel("DSN");
    sqlDsnLabel->setObjectName("sqlDsnLabel");
    sqlConnGrid->addWidget(sqlDsnLabel, 1, 0);
    _sqlDsnEdit = new QLineEdit("MuOnline", sqlConnBox);
    _sqlDsnEdit->setMaximumWidth(220);
    sqlConnGrid->addWidget(_sqlDsnEdit, 1, 1);
    _sqlWindowsAuthCheck = new QCheckBox(l("Windows-аутентификация", "Windows authentication"), sqlConnBox);
    sqlConnGrid->addWidget(_sqlWindowsAuthCheck, 1, 2);
    _sqlTestButton = new QPushButton(sqlConnBox);
    _sqlTestButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    _sqlTestButton->setToolTip(l("Проверить ODBC подключение", "Test ODBC connection"));
    _sqlTestButton->setFixedWidth(30);
    sqlConnGrid->addWidget(_sqlTestButton, 1, 3);

    auto* sqlDbLabel = new QLabel(l("База", "Database"));
    sqlDbLabel->setObjectName("sqlDbLabel");
    sqlConnGrid->addWidget(sqlDbLabel, 2, 0);
    _sqlDatabaseEdit = new QLineEdit("MuOnline", sqlConnBox);
    _sqlDatabaseEdit->setMaximumWidth(220);
    sqlConnGrid->addWidget(_sqlDatabaseEdit, 2, 1);
    auto* sqlLoginLabel = new QLabel(l("Логин", "Login"));
    sqlLoginLabel->setObjectName("sqlLoginLabel");
    sqlConnGrid->addWidget(sqlLoginLabel, 2, 2);
    _sqlLoginEdit = new QLineEdit("sa", sqlConnBox);
    _sqlLoginEdit->setMaximumWidth(170);
    sqlConnGrid->addWidget(_sqlLoginEdit, 2, 3);

    auto* sqlPasswordLabel = new QLabel(l("Пароль", "Password"));
    sqlPasswordLabel->setObjectName("sqlPasswordLabel");
    sqlConnGrid->addWidget(sqlPasswordLabel, 3, 2);
    _sqlPasswordEdit = new QLineEdit(sqlConnBox);
    _sqlPasswordEdit->setEchoMode(QLineEdit::Password);
    _sqlPasswordEdit->setMaximumWidth(170);
    sqlConnGrid->addWidget(_sqlPasswordEdit, 3, 3);

    connect(_sqlTestButton, &QPushButton::clicked, this, &MainWindow::testSqlConnection);
    connect(_sqlDetectButton, &QPushButton::clicked, this, &MainWindow::detectSqlServerAuto);
    connect(_sqlApplyOdbcButton, &QPushButton::clicked, this, &MainWindow::loadOdbcFromConfigs);
    connect(_sqlWindowsAuthCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        _sqlLoginEdit->setEnabled(!checked);
        _sqlPasswordEdit->setEnabled(!checked);
    });

    sqlLayout->addWidget(sqlConnBox);

    auto* sqlConfigBox = new QGroupBox(l("SQL конфиги", "SQL configs"), sqlTab);
    sqlConfigBox->setObjectName("sqlConfigBox");
    auto* sqlConfigLayout = new QVBoxLayout(sqlConfigBox);
    auto* sqlButtons = new QWidget(sqlConfigBox);
    auto* sqlButtonsLayout = new QHBoxLayout(sqlButtons);
    sqlButtonsLayout->setContentsMargins(0, 0, 0, 0);
    sqlButtonsLayout->setSpacing(6);
    _scanSqlConfigButton = new QPushButton(sqlButtons);
    _scanSqlConfigButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    _scanSqlConfigButton->setToolTip(l("Сканировать SQL конфиги", "Scan SQL configs"));
    _scanSqlConfigButton->setFixedWidth(30);
    sqlButtonsLayout->addWidget(_scanSqlConfigButton);
    auto* sqlOldPasswordLabel = new QLabel(l("Старый пароль", "Old password"), sqlButtons);
    sqlOldPasswordLabel->setObjectName("sqlOldPasswordLabel");
    sqlButtonsLayout->addWidget(sqlOldPasswordLabel);
    _oldSqlPasswordEdit = new QLineEdit(sqlButtons);
    _oldSqlPasswordEdit->setEchoMode(QLineEdit::Password);
    _oldSqlPasswordEdit->setMinimumWidth(110);
    sqlButtonsLayout->addWidget(_oldSqlPasswordEdit);
    auto* sqlNewPasswordLabel = new QLabel(l("Новый пароль", "New password"), sqlButtons);
    sqlNewPasswordLabel->setObjectName("sqlNewPasswordLabel");
    sqlButtonsLayout->addWidget(sqlNewPasswordLabel);
    _newSqlPasswordEdit = new QLineEdit(sqlButtons);
    _newSqlPasswordEdit->setEchoMode(QLineEdit::Password);
    _newSqlPasswordEdit->setMinimumWidth(110);
    sqlButtonsLayout->addWidget(_newSqlPasswordEdit);
    _sqlBackupCheck = new QCheckBox(l("Резервная копия перед изменением", "Backup before changes"), sqlButtons);
    _sqlBackupCheck->setChecked(true);
    sqlButtonsLayout->addWidget(_sqlBackupCheck);
    _updateSqlPasswordButton = new QPushButton(sqlButtons);
    _updateSqlPasswordButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    _updateSqlPasswordButton->setToolTip(l("Обновить SQL пароль в выбранных файлах", "Update SQL password in selected files"));
    _updateSqlPasswordButton->setFixedWidth(30);
    sqlButtonsLayout->addWidget(_updateSqlPasswordButton);
    sqlButtonsLayout->addStretch();
    sqlConfigLayout->addWidget(sqlButtons);

    _sqlConfigFilesTable = new QTableWidget(0, 4, sqlConfigBox);
    _sqlConfigFilesTable->setHorizontalHeaderLabels({l("Обновить", "Apply"), l("Совпадений", "Matches"), l("Файл", "File"), l("Пример", "Sample")});
    _sqlConfigFilesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    _sqlConfigFilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _sqlConfigFilesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _sqlConfigFilesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    _sqlConfigFilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _sqlConfigFilesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _sqlConfigFilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sqlConfigLayout->addWidget(_sqlConfigFilesTable, 1);

    connect(_scanSqlConfigButton, &QPushButton::clicked, this, &MainWindow::scanSqlConfigFiles);
    connect(_updateSqlPasswordButton, &QPushButton::clicked, this, &MainWindow::updateSqlPasswordInConfigs);

    sqlLayout->addWidget(sqlConfigBox, 1);

    _sqlTabIndex = tabs->addTab(sqlTab, l("Config SQL", "SQL Config"));
}

void MainWindow::setupRunTab(QTabWidget* tabs)
{
    auto* runTab = new QWidget(tabs);
    auto* runLayout = new QVBoxLayout(runTab);
    runLayout->setContentsMargins(2, 2, 2, 2);
    runLayout->setSpacing(4);

    auto* profileRow = new QWidget(runTab);
    auto* profileLayout = new QHBoxLayout(profileRow);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(6);
    auto* profileLabel = new QLabel(l("Профиль", "Profile"), profileRow);
    profileLabel->setObjectName("profileLabel");
    profileLayout->addWidget(profileLabel);
    _profileCombo = new QComboBox(profileRow);
    _profileCombo->addItem(l("Локальный", "Local"), "local");
    _profileCombo->addItem(l("Тестовый", "Test"), "test");
    _profileCombo->addItem(l("Продакшн", "Prod"), "prod");
    _profileCombo->setMaximumWidth(150);
    profileLayout->addWidget(_profileCombo);
    _applyProfileButton = new QPushButton(profileRow);
    _applyProfileButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    _applyProfileButton->setFixedWidth(30);
    _applyProfileButton->setToolTip(l("Применить профиль", "Apply profile"));
    profileLayout->addWidget(_applyProfileButton);
    _saveProfileButton = new QPushButton(profileRow);
    _saveProfileButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    _saveProfileButton->setFixedWidth(30);
    _saveProfileButton->setToolTip(l("Сохранить в профиль", "Save to profile"));
    profileLayout->addWidget(_saveProfileButton);
    profileLayout->addStretch();
    runLayout->addWidget(profileRow);

    auto* runButtons = new QWidget(runTab);
    auto* runButtonsLayout = new QHBoxLayout(runButtons);
    runButtonsLayout->setContentsMargins(0, 0, 0, 0);
    _addExeButton = new QPushButton;
    _addExeButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    _addExeButton->setToolTip(l("Добавить EXE", "Add EXE"));
    _removeExeButton = new QPushButton;
    _removeExeButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    _removeExeButton->setToolTip(l("Удалить выбранный процесс", "Remove selected process"));
    _startAllButton = new QPushButton;
    _startAllButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    _startAllButton->setToolTip(l("Запустить все включенные процессы", "Start all enabled processes"));
    _stopAllButton = new QPushButton;
    _stopAllButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    _stopAllButton->setToolTip(l("Остановить все процессы", "Stop all processes"));
    _saveProcessesButton = new QPushButton;
    _saveProcessesButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    _saveProcessesButton->setToolTip(l("Сохранить список процессов", "Save process list"));
    const auto compactButtons = {_addExeButton, _removeExeButton, _startAllButton, _stopAllButton, _saveProcessesButton};
    for (auto* button : compactButtons)
    {
        button->setFixedWidth(30);
    }
    runButtonsLayout->addWidget(_addExeButton);
    runButtonsLayout->addWidget(_removeExeButton);
    runButtonsLayout->addSpacing(12);
    runButtonsLayout->addWidget(_startAllButton);
    runButtonsLayout->addWidget(_stopAllButton);
    runButtonsLayout->addSpacing(12);
    runButtonsLayout->addWidget(_saveProcessesButton);
    runButtonsLayout->addStretch();
    runLayout->addWidget(runButtons);

    _processesTable = new QTableWidget(0, 8, runTab);
    _processesTable->setHorizontalHeaderLabels({l("Вкл", "On"), l("Рестарт", "Restart"), l("Имя", "Name"), l("Путь EXE", "EXE path"), l("Статус", "Status"), l("Окно", "Window"), l("Порядок", "Order"), l("Задержка мс", "Delay ms")});
    _processesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    _processesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    _processesTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    _processesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _processesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    runLayout->addWidget(_processesTable, 1);

    auto* runBottom = new QGroupBox(l("Мониторинг", "Monitoring"), runTab);
    runBottom->setObjectName("runMonitorBox");
    auto* runBottomLayout = new QVBoxLayout(runBottom);
    _startWithWindowsCheck = new QCheckBox(l("Запускать утилиту при старте Windows", "Start utility with Windows"), runBottom);
    runBottomLayout->addWidget(_startWithWindowsCheck);
    _minimizeToTrayAfterStartCheck = new QCheckBox(l("После старта всех сервисов свернуть в трей", "Minimize to tray after all services start"), runBottom);
    runBottomLayout->addWidget(_minimizeToTrayAfterStartCheck);
    _closeToTrayOnCloseCheck = new QCheckBox(l("При нажатии X сворачивать в трей", "Minimize to tray on X button"), runBottom);
    runBottomLayout->addWidget(_closeToTrayOnCloseCheck);
    _stopProcessesOnCloseCheck = new QCheckBox(l("При закрытии завершать все процессы", "Stop all processes on app close"), runBottom);
    runBottomLayout->addWidget(_stopProcessesOnCloseCheck);
    auto* monitorHint = new QLabel(l("Мониторинг процессов и автоперезапуск при падении.", "Process monitoring and auto-restart on crash."));
    monitorHint->setObjectName("runMonitorHint");
    runBottomLayout->addWidget(monitorHint);
    runLayout->addWidget(runBottom);

    connect(_addExeButton, &QPushButton::clicked, this, &MainWindow::addProcessFromDialog);
    connect(_removeExeButton, &QPushButton::clicked, this, &MainWindow::removeSelectedProcess);
    connect(_startAllButton, &QPushButton::clicked, this, &MainWindow::startAllProcesses);
    connect(_stopAllButton, &QPushButton::clicked, this, &MainWindow::stopAllProcesses);
    connect(_saveProcessesButton, &QPushButton::clicked, this, [this]()
    {
        saveProcessSettings();
        statusBar()->showMessage(l("Список процессов сохранён", "Process list saved"));
    });
    connect(_applyProfileButton, &QPushButton::clicked, this, [this]()
    {
        if (_profileCombo)
        {
            applyProfile(_profileCombo->currentData().toString());
        }
    });
    connect(_saveProfileButton, &QPushButton::clicked, this, [this]()
    {
        if (_profileCombo)
        {
            saveCurrentToProfile(_profileCombo->currentData().toString());
        }
    });
    connect(_profileCombo, &QComboBox::currentIndexChanged, this, [this](int)
    {
        saveSettingsSoon();
    });
    connect(_processesTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
    {
        if (!item || _isLoadingSettings)
        {
            return;
        }
        if (item->column() == 5)
        {
            const int row = item->row();
            const QString key = keyForRow(row);
            auto* process = _runningProcesses.value(key, nullptr);
            if (process && process->state() != QProcess::NotRunning)
            {
                applyProcessWindowVisibility(static_cast<qint64>(process->processId()), tableCheckState(item));
            }
        }
        if (item->column() == 6 || item->column() == 7)
        {
            bool ok = false;
            int value = item->text().trimmed().toInt(&ok);
            if (!ok)
            {
                value = item->column() == 6 ? 1 : 0;
            }
            if (value < 0)
            {
                value = 0;
            }
            QSignalBlocker blocker(_processesTable);
            item->setText(QString::number(value));
        }
        saveSettingsSoon();
    });
    connect(_startWithWindowsCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        if (!setStartWithWindows(checked))
        {
            _startWithWindowsCheck->setChecked(isStartWithWindowsEnabled());
            statusBar()->showMessage(l("Не удалось обновить автозапуск Windows", "Failed to update Windows autostart"));
            return;
        }
        saveSettingsSoon();
    });
    connect(_minimizeToTrayAfterStartCheck, &QCheckBox::toggled, this, [this](bool)
    {
        saveSettingsSoon();
    });
    connect(_closeToTrayOnCloseCheck, &QCheckBox::toggled, this, [this](bool)
    {
        saveSettingsSoon();
    });
    connect(_stopProcessesOnCloseCheck, &QCheckBox::toggled, this, [this](bool)
    {
        saveSettingsSoon();
    });

    _runTabIndex = tabs->addTab(runTab, l("Автозапуск", "Autostart"));
}

void MainWindow::addProcessRow(const QString& name, const QString& relativePath, bool enabled, bool autoRestart, const QString& status, int startOrder, int startDelayMs)
{
    const int row = _processesTable->rowCount();
    _processesTable->insertRow(row);
    _processesTable->setItem(row, 0, makeCheckItem(enabled));
    _processesTable->setItem(row, 1, makeCheckItem(autoRestart));
    _processesTable->setItem(row, 2, new QTableWidgetItem(name));
    _processesTable->setItem(row, 3, new QTableWidgetItem(relativePath));
    _processesTable->setItem(row, 4, makeReadOnlyItem(status));
    _processesTable->setItem(row, 5, makeCheckItem(false));
    _processesTable->setItem(row, 6, new QTableWidgetItem(QString::number(startOrder)));
    _processesTable->setItem(row, 7, new QTableWidgetItem(QString::number(startDelayMs)));
}

QString MainWindow::settingsPath() const
{
    return QCoreApplication::applicationDirPath() + "/serverquickmanagerqt.settings.json";
}

void MainWindow::loadProcessSettings()
{
    _isLoadingSettings = true;
    QSignalBlocker blocker(_processesTable);

    QJsonObject rootObject;
    QFile file(settingsPath());
    if (file.open(QIODevice::ReadOnly))
    {
        const auto doc = QJsonDocument::fromJson(file.readAll());
        rootObject = doc.object();
        const auto arr = rootObject.value("processes").toArray();
        for (const auto& value : arr)
        {
            const auto obj = value.toObject();
            addProcessRow(
                obj.value("name").toString(),
                obj.value("relativePath").toString(),
                obj.value("isEnabled").toBool(true),
                obj.value("autoRestart").toBool(true),
                l("Остановлен", "Stopped"),
                obj.value("startOrder").toInt(_processesTable->rowCount() + 1),
                obj.value("startDelayMs").toInt(400));
            const int row = _processesTable->rowCount() - 1;
            _processesTable->item(row, 5)->setCheckState(obj.value("showWindow").toBool(false) ? Qt::Checked : Qt::Unchecked);
        }
    }

    if (!rootObject.isEmpty())
    {
        const QString serverRoot = rootObject.value("serverRoot").toString();
        if (!serverRoot.isEmpty() && QDir(serverRoot).exists())
        {
            _repoRoot = serverRoot;
            _serverRootEdit->setText(_repoRoot);
        }
        _publicIpEdit->setText(rootObject.value("lastPublicIp").toString());
        _sqlServerEdit->setText(rootObject.value("sqlServer").toString(".\\SQLEXPRESS"));
        _sqlDsnEdit->setText(rootObject.value("sqlDsn").toString("MuOnline"));
        _sqlDatabaseEdit->setText(rootObject.value("sqlDatabase").toString("MuOnline"));
        _sqlLoginEdit->setText(rootObject.value("sqlLogin").toString("sa"));
        _sqlPasswordEdit->setText(rootObject.value("sqlPassword").toString());
        _sqlWindowsAuthCheck->setChecked(rootObject.value("sqlWindowsAuth").toBool(false));
        _skipLocalIpCheck->setChecked(rootObject.value("skipLocalIp").toBool(true));
        _createBackupCheck->setChecked(rootObject.value("createIpBackup").toBool(true));
        _sqlBackupCheck->setChecked(rootObject.value("createSqlBackup").toBool(true));
        _minimizeToTrayAfterStartCheck->setChecked(rootObject.value("minimizeToTrayAfterStart").toBool(false));
        _closeToTrayOnCloseCheck->setChecked(rootObject.value("closeToTrayOnClose").toBool(false));
        _stopProcessesOnCloseCheck->setChecked(rootObject.value("stopProcessesOnClose").toBool(false));
        _closeBehaviorPromptShown = rootObject.value("closeBehaviorPromptShown").toBool(false);
        _profiles = rootObject.value("profiles").toObject();
        _currentLanguage = rootObject.value("language").toString("ru");
        _currentTheme = rootObject.value("theme").toString("dark");
        if (rootObject.contains("startWithWindows"))
        {
            const bool shouldStartWithWindows = rootObject.value("startWithWindows").toBool(false);
            setStartWithWindows(shouldStartWithWindows);
        }
    }
    _startWithWindowsCheck->setChecked(isStartWithWindowsEnabled());

    if (_processesTable->rowCount() == 0)
    {
        addProcessRow("ConnectServer", ".\\1.ConnectServer\\ConnectServer.exe", true, true, l("Остановлен", "Stopped"), 1, 300);
        addProcessRow("DataServer", ".\\2.DataServer\\DataServer.exe", true, true, l("Остановлен", "Stopped"), 2, 300);
        addProcessRow("JoinServer", ".\\3.JoinServer\\JoinServer.exe", true, true, l("Остановлен", "Stopped"), 3, 300);
        addProcessRow("GameServer", ".\\4.MuServer\\Sub-1\\GameServer\\GameServer.exe", true, true, l("Остановлен", "Stopped"), 4, 500);
        addProcessRow("GameServerCS", ".\\4.MuServer\\Sub-1\\GameServerCS\\GameServer.exe", false, true, l("Остановлен", "Stopped"), 5, 500);
    }

    if (_profiles.isEmpty())
    {
        const QJsonObject current = buildCurrentProfileState();
        _profiles.insert("local", current);
        _profiles.insert("test", current);
        _profiles.insert("prod", current);
    }
    if (_profileCombo)
    {
        const QString selectedProfile = rootObject.value("selectedProfile").toString("local");
        const int profileIndex = _profileCombo->findData(selectedProfile);
        _profileCombo->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
    }

    _isLoadingSettings = false;
    applyLanguage(_currentLanguage);
    applyTheme(_currentTheme);
}

void MainWindow::saveProcessSettings() const
{
    if (_isLoadingSettings)
    {
        return;
    }

    QJsonArray processes;
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        QJsonObject obj;
        obj["name"] = _processesTable->item(row, 2)->text();
        obj["relativePath"] = _processesTable->item(row, 3)->text();
        obj["isEnabled"] = tableCheckState(_processesTable->item(row, 0));
        obj["autoRestart"] = tableCheckState(_processesTable->item(row, 1));
        obj["showWindow"] = tableCheckState(_processesTable->item(row, 5));
        obj["startOrder"] = _processesTable->item(row, 6)->text().toInt();
        obj["startDelayMs"] = _processesTable->item(row, 7)->text().toInt();
        processes.append(obj);
    }
    QJsonObject root;
    root["processes"] = processes;
    root["serverRoot"] = _repoRoot;
    root["lastPublicIp"] = _publicIpEdit ? _publicIpEdit->text().trimmed() : QString();
    root["sqlServer"] = _sqlServerEdit ? _sqlServerEdit->text().trimmed() : QString();
    root["sqlDsn"] = _sqlDsnEdit ? _sqlDsnEdit->text().trimmed() : QString();
    root["sqlDatabase"] = _sqlDatabaseEdit ? _sqlDatabaseEdit->text().trimmed() : QString();
    root["sqlLogin"] = _sqlLoginEdit ? _sqlLoginEdit->text().trimmed() : QString();
    root["sqlPassword"] = _sqlPasswordEdit ? _sqlPasswordEdit->text() : QString();
    root["sqlWindowsAuth"] = _sqlWindowsAuthCheck ? _sqlWindowsAuthCheck->isChecked() : false;
    root["skipLocalIp"] = _skipLocalIpCheck ? _skipLocalIpCheck->isChecked() : true;
    root["createIpBackup"] = _createBackupCheck ? _createBackupCheck->isChecked() : true;
    root["createSqlBackup"] = _sqlBackupCheck ? _sqlBackupCheck->isChecked() : true;
    root["startWithWindows"] = _startWithWindowsCheck ? _startWithWindowsCheck->isChecked() : false;
    root["minimizeToTrayAfterStart"] = _minimizeToTrayAfterStartCheck ? _minimizeToTrayAfterStartCheck->isChecked() : false;
    root["closeToTrayOnClose"] = _closeToTrayOnCloseCheck ? _closeToTrayOnCloseCheck->isChecked() : false;
    root["stopProcessesOnClose"] = _stopProcessesOnCloseCheck ? _stopProcessesOnCloseCheck->isChecked() : false;
    root["closeBehaviorPromptShown"] = _closeBehaviorPromptShown;
    root["profiles"] = _profiles;
    root["selectedProfile"] = _profileCombo ? _profileCombo->currentData().toString() : "local";
    root["language"] = _currentLanguage;
    root["theme"] = _currentTheme;

    QFile file(settingsPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

QString MainWindow::keyForRow(int row) const
{
    return _processesTable->item(row, 3)->text().trimmed().toLower();
}

void MainWindow::startAllProcesses()
{
    struct StartPlan
    {
        int row = -1;
        int order = 0;
        int delayMs = 0;
    };

    std::vector<StartPlan> plans;
    plans.reserve(_processesTable->rowCount());
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        if (tableCheckState(_processesTable->item(row, 0)))
        {
            bool orderOk = false;
            bool delayOk = false;
            int order = _processesTable->item(row, 6)->text().toInt(&orderOk);
            int delayMs = _processesTable->item(row, 7)->text().toInt(&delayOk);
            if (!orderOk) order = row + 1;
            if (!delayOk || delayMs < 0) delayMs = 0;
            plans.push_back(StartPlan{row, order, delayMs});
        }
    }
    std::sort(plans.begin(), plans.end(), [](const StartPlan& a, const StartPlan& b)
    {
        if (a.order != b.order) return a.order < b.order;
        return a.row < b.row;
    });

    int cumulativeDelay = 0;
    for (const auto& plan : plans)
    {
        QTimer::singleShot(cumulativeDelay, this, [this, row = plan.row]()
        {
            startProcessForRow(row, true);
        });
        cumulativeDelay += plan.delayMs;
    }

    statusBar()->showMessage(l("Запуск всех выполнен", "Start all completed"));
    if (_minimizeToTrayAfterStartCheck && _minimizeToTrayAfterStartCheck->isChecked() && !plans.empty())
    {
        QTimer::singleShot(cumulativeDelay + 500, this, [this]()
        {
            if (areAllEnabledProcessesRunning())
            {
                minimizeToTray();
            }
        });
    }
}

void MainWindow::stopAllProcesses()
{
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        stopProcessForRow(row);
    }
    statusBar()->showMessage(l("Остановка всех выполнена", "Stop all completed"));
}

void MainWindow::startProcessForRow(int row, bool manualStart)
{
    const QString relativePath = _processesTable->item(row, 3)->text().trimmed();
    const QString key = keyForRow(row);
    const QString fullPath = QDir(_repoRoot).filePath(relativePath);

    if (fullPath.isEmpty() || !QFile::exists(fullPath))
    {
        _processesTable->item(row, 4)->setText(l("EXE не найден", "EXE not found"));
        return;
    }

    if (!manualStart)
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const auto last = _lastStartAttempts.value(key);
        if (last.isValid() && last.msecsTo(now) < 2500)
        {
            return;
        }
        _lastStartAttempts.insert(key, now);
    }
    else
    {
        _restartBlockedUntil.remove(key);
        _restartAttemptsInWindow.remove(key);
        _restartWindowStart.remove(key);
    }

    if (_runningProcesses.contains(key))
    {
        auto* existing = _runningProcesses.value(key);
        if (existing && existing->state() != QProcess::NotRunning)
        {
            return;
        }
        _runningProcesses.remove(key);
    }

    auto* process = new QProcess(this);
    process->setProgram(fullPath);
    process->setWorkingDirectory(QFileInfo(fullPath).absolutePath());
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args)
    {
        args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        args->startupInfo->wShowWindow = SW_HIDE;
    });
#endif
    process->start();
    if (!process->waitForStarted(2000))
    {
        _processesTable->item(row, 4)->setText(l("Ошибка запуска", "Start failed"));
        process->deleteLater();
        return;
    }

    _runningProcesses.insert(key, process);
    const bool showWindow = tableCheckState(_processesTable->item(row, 5));
    applyProcessWindowVisibility(static_cast<qint64>(process->processId()), showWindow);
    _processesTable->item(row, 4)->setText(QString("%1 (%2) %3").arg(l("Работает", "Running")).arg(process->processId()).arg(showWindow ? l("Видим", "Visible") : l("Скрыт", "Hidden")));
    saveSettingsSoon();
}

void MainWindow::stopProcessForRow(int row)
{
    const QString key = keyForRow(row);
    auto* process = _runningProcesses.value(key, nullptr);
    if (process)
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
            process->waitForFinished(3000);
        }
        process->deleteLater();
        _runningProcesses.remove(key);
    }
    _processesTable->item(row, 4)->setText(l("Остановлен", "Stopped"));
    saveSettingsSoon();
}

void MainWindow::monitorProcesses()
{
    const auto now = QDateTime::currentDateTimeUtc();
    constexpr int restartWindowSec = 60;
    constexpr int restartLimit = 5;
    constexpr int blockSec = 120;
    for (int row = 0; row < _processesTable->rowCount(); ++row)
    {
        const QString key = keyForRow(row);
        auto* process = _runningProcesses.value(key, nullptr);
        const bool enabled = tableCheckState(_processesTable->item(row, 0));
        const bool autoRestart = tableCheckState(_processesTable->item(row, 1));
        const bool showWindow = tableCheckState(_processesTable->item(row, 5));
        if (process && process->state() != QProcess::NotRunning)
        {
            _restartAttemptsInWindow.remove(key);
            _restartWindowStart.remove(key);
            _restartBlockedUntil.remove(key);
            applyProcessWindowVisibility(static_cast<qint64>(process->processId()), showWindow);
            _processesTable->item(row, 4)->setText(QString("%1 (%2) %3").arg(l("Работает", "Running")).arg(process->processId()).arg(showWindow ? l("Видим", "Visible") : l("Скрыт", "Hidden")));
            continue;
        }

        if (process)
        {
            process->deleteLater();
            _runningProcesses.remove(key);
        }

        _processesTable->item(row, 4)->setText(l("Остановлен", "Stopped"));
        if (enabled && autoRestart)
        {
            const auto blockedUntil = _restartBlockedUntil.value(key);
            if (blockedUntil.isValid() && now < blockedUntil)
            {
                const int waitSec = std::max(1, static_cast<int>(now.secsTo(blockedUntil)));
                _processesTable->item(row, 4)->setText(QString("%1 %2c").arg(l("Crash-loop пауза", "Crash-loop pause")).arg(waitSec));
                continue;
            }
            if (blockedUntil.isValid() && now >= blockedUntil)
            {
                _restartBlockedUntil.remove(key);
                _restartAttemptsInWindow.remove(key);
                _restartWindowStart.remove(key);
            }

            const auto windowStart = _restartWindowStart.value(key);
            if (!windowStart.isValid() || windowStart.secsTo(now) > restartWindowSec)
            {
                _restartWindowStart.insert(key, now);
                _restartAttemptsInWindow.insert(key, 0);
            }
            const int attempts = _restartAttemptsInWindow.value(key) + 1;
            _restartAttemptsInWindow.insert(key, attempts);
            if (attempts > restartLimit)
            {
                _restartBlockedUntil.insert(key, now.addSecs(blockSec));
                _processesTable->item(row, 4)->setText(l("Crash-loop блок 120с", "Crash-loop blocked 120s"));
                continue;
            }
            startProcessForRow(row, false);
        }
    }
}

void MainWindow::addProcessFromDialog()
{
    const QString file = QFileDialog::getOpenFileName(this, l("Выберите EXE", "Select EXE"), _repoRoot, "Executable (*.exe)");
    if (file.isEmpty())
    {
        return;
    }
    QString relative = QDir(_repoRoot).relativeFilePath(file);
    if (!relative.startsWith(".\\") && !relative.startsWith("./"))
    {
        relative = ".\\" + relative;
    }
    addProcessRow(QFileInfo(file).baseName(), QDir::toNativeSeparators(relative), true, true, l("Остановлен", "Stopped"), _processesTable->rowCount() + 1, 400);
    saveSettingsSoon();
}

void MainWindow::removeSelectedProcess()
{
    const int row = _processesTable->currentRow();
    if (row < 0)
    {
        return;
    }
    stopProcessForRow(row);
    _processesTable->removeRow(row);
    saveSettingsSoon();
}

void MainWindow::testSqlConnection()
{
    const QString dsn = _sqlDsnEdit->text().trimmed();
    const QString sqlServer = _sqlServerEdit->text().trimmed();
    const QString database = _sqlDatabaseEdit->text().trimmed();
    const QString login = _sqlLoginEdit->text().trimmed();
    const QString password = _sqlPasswordEdit->text();
    const bool windowsAuth = _sqlWindowsAuthCheck->isChecked();

    if (dsn.isEmpty() && sqlServer.isEmpty())
    {
        statusBar()->showMessage(l("Укажите DSN или SQL Server", "Specify DSN or SQL Server"));
        return;
    }
    if (!windowsAuth && (login.isEmpty() || password.isEmpty()))
    {
        statusBar()->showMessage(l("Укажите SQL login/password", "Specify SQL login/password"));
        return;
    }

    auto buildAuthPart = [&]() -> QString
    {
        if (windowsAuth)
        {
            return "Trusted_Connection=Yes;";
        }
        return QString("UID=%1;PWD=%2;").arg(login, password);
    };

    QStringList attempts;
    if (!dsn.isEmpty())
    {
        QString connectionString = QString("DSN=%1;").arg(dsn);
        if (!database.isEmpty())
        {
            connectionString += QString("DATABASE=%1;").arg(database);
        }
        connectionString += buildAuthPart();
        attempts.append(connectionString);
    }

    if (!sqlServer.isEmpty())
    {
        const QStringList driverCandidates = {
            "ODBC Driver 18 for SQL Server",
            "ODBC Driver 17 for SQL Server",
            "SQL Server"
        };
        for (const auto& driverName : driverCandidates)
        {
            QString connectionString = QString("DRIVER={%1};SERVER=%2;").arg(driverName, sqlServer);
            if (!database.isEmpty())
            {
                connectionString += QString("DATABASE=%1;").arg(database);
            }
            connectionString += "TrustServerCertificate=Yes;";
            connectionString += buildAuthPart();
            attempts.append(connectionString);
        }
    }

    QString lastError;
    QString usedConnection;
    bool ok = false;
    for (int i = 0; i < attempts.size(); ++i)
    {
        const QString connectionName = QString("odbc_test_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(i);
        const QString connectionString = attempts[i];

        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", connectionName);
            db.setDatabaseName(connectionString);
            if (!db.open())
            {
                lastError = db.lastError().text();
            }
            else
            {
                QSqlQuery query(db);
                if (query.exec("SELECT 1"))
                {
                    ok = true;
                    usedConnection = connectionString;
                }
                else
                {
                    lastError = query.lastError().text();
                }
            }
        }
        QSqlDatabase::removeDatabase(connectionName);

        if (ok)
        {
            break;
        }
    }

    if (ok)
    {
        if (usedConnection.startsWith("DSN=", Qt::CaseInsensitive))
        {
            statusBar()->showMessage(QString("%1 (DSN: %2)").arg(l("SQL подключение успешно", "SQL connection successful"), dsn));
        }
        else
        {
            statusBar()->showMessage(QString("%1 (SERVER: %2)").arg(l("SQL подключение успешно", "SQL connection successful"), sqlServer));
        }
        return;
    }

    QString friendly = lastError;
    if (lastError.contains("IM002", Qt::CaseInsensitive))
    {
        friendly = l("ODBC источник данных не найден. Проверь DSN и наличие драйвера SQL Server. Можно оставить DSN пустым и использовать поле SQL Server.",
                     "ODBC data source was not found. Check DSN and SQL Server driver availability. You can leave DSN empty and use SQL Server field.");
    }
    statusBar()->showMessage(QString("%1: %2").arg(l("SQL ошибка", "SQL error"), friendly));
    QMessageBox::warning(this, l("SQL ошибка", "SQL error"), friendly);
}

void MainWindow::loadOdbcFromConfigs()
{
    QString dsn;
    QString user;
    QString pass;
    const auto files = enumerateOdbcConfigFiles(_repoRoot);
    for (const auto& file : files)
    {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
        {
            continue;
        }
        const QString content = QString::fromUtf8(f.readAll());
        if (dsn.isEmpty())
        {
            dsn = readIniValue(content, "JoinServerODBC");
            if (dsn.isEmpty())
            {
                dsn = readIniValue(content, "DataServerODBC");
            }
        }
        if (user.isEmpty())
        {
            user = readIniValue(content, "JoinServerUSER");
            if (user.isEmpty())
            {
                user = readIniValue(content, "DataServerUSER");
            }
        }
        if (pass.isEmpty())
        {
            pass = readIniValue(content, "JoinServerPASS");
            if (pass.isEmpty())
            {
                pass = readIniValue(content, "DataServerPASS");
            }
        }
    }

    if (!dsn.isEmpty())
    {
        _sqlDsnEdit->setText(dsn);
    }
    if (!user.isEmpty())
    {
        _sqlLoginEdit->setText(user);
    }
    if (!pass.isEmpty())
    {
        _sqlPasswordEdit->setText(pass);
    }
    statusBar()->showMessage(dsn.isEmpty() ? l("DSN в конфигах не найден", "DSN was not found in configs") : QString("%1: %2").arg(l("DSN загружен", "DSN loaded"), dsn));
}

void MainWindow::detectSqlServerAuto()
{
    if (_sqlDetectButton)
    {
        _sqlDetectButton->setEnabled(false);
    }
    statusBar()->showMessage(l("Поиск SQL Server...", "Searching SQL Server..."));

    const QStringList servers = detectSqlServers();
    if (servers.isEmpty())
    {
        statusBar()->showMessage(l("SQL Server не найден", "SQL Server not found"));
    }
    else
    {
        _sqlServerEdit->setText(servers.first());
        statusBar()->showMessage(QString("SQL Server: %1").arg(servers.join(", ")));
    }

    if (_sqlDetectButton)
    {
        _sqlDetectButton->setEnabled(true);
    }
}

QStringList MainWindow::detectSqlServers() const
{
    QSet<QString> results;
    addSqlInstancesFromRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL", results);
    addSqlInstancesFromRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL", results);
    addSqlInstancesFromRegistry("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL", results);
    addSqlInstancesFromRegistry("HKEY_CURRENT_USER\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SQL Server\\Instance Names\\SQL", results);
    addInstalledSqlInstances("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SQL Server", results);
    addInstalledSqlInstances("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SQL Server", results);
    addInstalledSqlInstances("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Microsoft SQL Server", results);
    addInstalledSqlInstances("HKEY_CURRENT_USER\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SQL Server", results);
    addSqlServices(results);
    addSqlServersFromSqlCmd(results);

    QStringList ordered = results.values();
    std::sort(ordered.begin(), ordered.end(), [](const QString& a, const QString& b)
    {
        auto rank = [](const QString& v) -> int
        {
            if (v == ".") return 0;
            if (v.startsWith(".\\", Qt::CaseInsensitive)) return 1;
            return 2;
        };
        const int ra = rank(a);
        const int rb = rank(b);
        if (ra != rb) return ra < rb;
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return ordered;
}

void MainWindow::addSqlInstancesFromRegistry(const QString& root, QSet<QString>& results)
{
    QSettings reg(root, QSettings::NativeFormat);
    const QStringList names = reg.allKeys();
    for (const QString& keyName : names)
    {
        const QString trimmed = keyName.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        if (trimmed.compare("MSSQLSERVER", Qt::CaseInsensitive) == 0)
        {
            results.insert(".");
        }
        else
        {
            results.insert(".\\" + trimmed);
        }
    }
}

void MainWindow::addInstalledSqlInstances(const QString& root, QSet<QString>& results)
{
    QSettings reg(root, QSettings::NativeFormat);
    QVariant instancesVariant = reg.value("InstalledInstances");
    QStringList instances;
    if (instancesVariant.typeId() == QMetaType::QStringList)
    {
        instances = instancesVariant.toStringList();
    }
    else if (instancesVariant.typeId() == QMetaType::QString)
    {
        const QString one = instancesVariant.toString().trimmed();
        if (!one.isEmpty())
        {
            instances.append(one);
        }
    }

    for (const QString& name : instances)
    {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        if (trimmed.compare("MSSQLSERVER", Qt::CaseInsensitive) == 0)
        {
            results.insert(".");
        }
        else
        {
            results.insert(".\\" + trimmed);
        }
    }
}

void MainWindow::addSqlServices(QSet<QString>& results)
{
    QSettings reg("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services", QSettings::NativeFormat);
    const QStringList services = reg.childGroups();
    for (const QString& serviceName : services)
    {
        const QString trimmed = serviceName.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        if (trimmed.compare("MSSQLSERVER", Qt::CaseInsensitive) == 0)
        {
            results.insert(".");
        }
        else if (trimmed.startsWith("MSSQL$", Qt::CaseInsensitive))
        {
            const QString instance = trimmed.mid(6).trimmed();
            if (!instance.isEmpty())
            {
                results.insert(".\\" + instance);
            }
        }
    }
}

void MainWindow::addSqlServersFromSqlCmd(QSet<QString>& results) const
{
    const QString sqlcmd = resolveSqlCmdPath();
    if (sqlcmd.isEmpty())
    {
        return;
    }

    QProcess process;
    process.setProgram(sqlcmd);
    process.setArguments({"-L"});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(1500))
    {
        return;
    }
    process.waitForFinished(4000);
    const QString output = QString::fromLocal8Bit(process.readAll());
    const QStringList lines = output.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (const QString& raw : lines)
    {
        const QString value = raw.trimmed();
        if (value.isEmpty() || value.startsWith("Servers", Qt::CaseInsensitive))
        {
            continue;
        }
        const QString normalized = normalizeSqlServerToken(value);
        if (!normalized.isEmpty())
        {
            results.insert(normalized);
        }
    }
}

QString MainWindow::normalizeSqlServerToken(const QString& value)
{
    const QStringList parts = value.split('\\', Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return {};
    }
    const QString serverName = parts[0].trimmed();
    const QString instanceName = parts.size() > 1 ? parts[1].trimmed() : QString();
    if (serverName.isEmpty())
    {
        return {};
    }
    const QString localName = QHostInfo::localHostName();
    const bool isLocal = serverName.compare(localName, Qt::CaseInsensitive) == 0
        || serverName == "."
        || serverName.compare("(local)", Qt::CaseInsensitive) == 0;

    if (instanceName.isEmpty())
    {
        return isLocal ? "." : serverName;
    }
    return isLocal ? ".\\" + instanceName : serverName + "\\" + instanceName;
}

QString MainWindow::resolveSqlCmdPath() const
{
    if (!_sqlCmdPath.isEmpty() && QFile::exists(_sqlCmdPath))
    {
        return _sqlCmdPath;
    }

    const QString pathEnv = qEnvironmentVariable("PATH");
    const QStringList pathParts = pathEnv.split(';', Qt::SkipEmptyParts);
    for (const QString& part : pathParts)
    {
        const QString candidate = QDir::fromNativeSeparators(part.trimmed());
        if (candidate.isEmpty())
        {
            continue;
        }
        const QString full = QDir(candidate).filePath("sqlcmd.exe");
        if (QFile::exists(full))
        {
            _sqlCmdPath = full;
            return _sqlCmdPath;
        }
    }

    const QStringList roots = {
        qEnvironmentVariable("ProgramFiles"),
        qEnvironmentVariable("ProgramFiles(x86)")
    };
    for (const QString& root : roots)
    {
        if (root.trimmed().isEmpty())
        {
            continue;
        }
        const QString baseDir = QDir::fromNativeSeparators(root) + "/Microsoft SQL Server";
        if (!QDir(baseDir).exists())
        {
            continue;
        }
        QDirIterator it(baseDir, {"sqlcmd.exe"}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
        {
            _sqlCmdPath = it.next();
            return _sqlCmdPath;
        }
    }

    return {};
}

void MainWindow::scanSqlConfigFiles()
{
    if (!QDir(_repoRoot).exists())
    {
        statusBar()->showMessage(l("Папка сервера не найдена", "Server folder not found"));
        return;
    }

    _scanSqlConfigButton->setEnabled(false);
    _sqlConfigFilesTable->setRowCount(0);
    statusBar()->showMessage(l("Сканирование SQL конфигов...", "Scanning SQL configs..."));

    const QRegularExpression sqlHintRegex(R"((?i)(Data Source|Initial Catalog|UID|User Id|PWD|Password|Server=|Database=|Trusted_Connection|SQLNCLI|SQLOLEDB))");
    const QStringList files = enumerateConfigFiles(_repoRoot);
    int row = 0;
    for (const auto& file : files)
    {
        QString content;
        QString encodingTag;
        bool hasUtf8Bom = false;
        if (!tryReadFile(file, content, encodingTag, hasUtf8Bom) || content.trimmed().isEmpty())
        {
            continue;
        }

        QRegularExpressionMatchIterator it = sqlHintRegex.globalMatch(content);
        int matchCount = 0;
        QString sample;
        while (it.hasNext())
        {
            const auto match = it.next();
            if (matchCount == 0)
            {
                sample = match.captured(0);
            }
            ++matchCount;
        }

        if (matchCount <= 0)
        {
            continue;
        }

        _sqlConfigFilesTable->insertRow(row);
        _sqlConfigFilesTable->setItem(row, 0, makeCheckItem(true));
        auto* countItem = new QTableWidgetItem(QString::number(matchCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        _sqlConfigFilesTable->setItem(row, 1, countItem);
        auto* pathItem = new QTableWidgetItem(QDir(_repoRoot).relativeFilePath(file));
        pathItem->setData(Qt::UserRole, file);
        _sqlConfigFilesTable->setItem(row, 2, pathItem);
        _sqlConfigFilesTable->setItem(row, 3, new QTableWidgetItem(sample));
        ++row;
    }

    _scanSqlConfigButton->setEnabled(true);
    statusBar()->showMessage(QString("%1: %2").arg(l("Найдено SQL конфигов", "SQL configs found"), QString::number(_sqlConfigFilesTable->rowCount())));
}

void MainWindow::updateSqlPasswordInConfigs()
{
    const QString oldPass = _oldSqlPasswordEdit->text();
    const QString newPass = _newSqlPasswordEdit->text();

    if (newPass.isEmpty())
    {
        statusBar()->showMessage(l("Введите новый пароль", "Enter new password"));
        return;
    }
    if (oldPass.isEmpty())
    {
        statusBar()->showMessage(l("Введите старый пароль", "Enter old password"));
        return;
    }

    const QRegularExpression passwordRegex(
        R"((?im)(?<key>PWD|Password|Pass|SqlPassword)(?<sep>\s*[:=]\s*)(?<quote>["']?)(?<val>[^;\r\n"']*)(?<endquote>["']?))");
    int changedFiles = 0;
    int changedValues = 0;
    QStringList reportRows;

    for (int row = 0; row < _sqlConfigFilesTable->rowCount(); ++row)
    {
        if (!tableCheckState(_sqlConfigFilesTable->item(row, 0)))
        {
            continue;
        }

        const QString absolutePath = _sqlConfigFilesTable->item(row, 2)->data(Qt::UserRole).toString();
        QString content;
        QString encodingTag;
        bool hasUtf8Bom = false;
        if (!tryReadFile(absolutePath, content, encodingTag, hasUtf8Bom))
        {
            continue;
        }

        int localChanged = 0;
        QString replaced;
        replaced.reserve(content.size());
        int lastPos = 0;
        auto it = passwordRegex.globalMatch(content);
        while (it.hasNext())
        {
            const QRegularExpressionMatch match = it.next();
            const int start = match.capturedStart();
            const int len = match.capturedLength();
            replaced += content.mid(lastPos, start - lastPos);

            const QString key = match.captured("key");
            const QString sep = match.captured("sep");
            const QString quote = match.captured("quote");
            const QString val = match.captured("val");
            const QString endQuote = match.captured("endquote");
            QString replacement = match.captured(0);
            if (val == oldPass)
            {
                replacement = key + sep + quote + newPass + endQuote;
                ++localChanged;
            }

            replaced += replacement;
            lastPos = start + len;
        }
        replaced += content.mid(lastPos);

        if (localChanged == 0)
        {
            continue;
        }

        if (_sqlBackupCheck->isChecked())
        {
            QFile::copy(absolutePath, makeBackupPath(absolutePath) + "_sql");
        }

        if (!writeFile(absolutePath, replaced, encodingTag, hasUtf8Bom))
        {
            continue;
        }

        changedFiles++;
        changedValues += localChanged;
        reportRows.append(QString("%1 — %2").arg(_sqlConfigFilesTable->item(row, 2)->text()).arg(localChanged));
    }

    statusBar()->showMessage(QString("%1. %2: %3, %4: %5")
                                 .arg(l("SQL пароль обновлён", "SQL password updated"))
                                 .arg(l("Файлов", "Files"))
                                 .arg(changedFiles)
                                 .arg(l("Замен", "Replacements"))
                                 .arg(changedValues));
    if (!reportRows.isEmpty())
    {
        QString report = reportRows.join("\n");
        QMessageBox::information(this, l("Отчёт SQL обновления", "SQL update report"), report);
    }
}

void MainWindow::scanIpFiles()
{
    if (!QDir(_repoRoot).exists())
    {
        statusBar()->showMessage(l("Папка сервера не найдена", "Server folder not found"));
        return;
    }

    _scanIpButton->setEnabled(false);
    _ipFilesTable->setRowCount(0);
    statusBar()->showMessage(l("Сканирование IP...", "Scanning IP..."));

    const QRegularExpression ipv4Regex(R"((?<!\d)([sS]?)(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d)\b)");
    const QStringList files = enumerateIpConfigFiles(_repoRoot);
    int row = 0;
    for (const auto& file : files)
    {
        QString content;
        QString encodingTag;
        bool hasUtf8Bom = false;
        if (!tryReadFile(file, content, encodingTag, hasUtf8Bom) || content.trimmed().isEmpty())
        {
            continue;
        }

        QRegularExpressionMatchIterator it = ipv4Regex.globalMatch(content);
        int matchCount = 0;
        QString sample;
        while (it.hasNext())
        {
            const auto match = it.next();
            if (matchCount == 0)
            {
                sample = match.captured(0);
            }
            ++matchCount;
        }

        if (matchCount <= 0)
        {
            continue;
        }

        _ipFilesTable->insertRow(row);
        _ipFilesTable->setItem(row, 0, makeCheckItem(true));
        auto* countItem = new QTableWidgetItem(QString::number(matchCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        _ipFilesTable->setItem(row, 1, countItem);
        auto* pathItem = new QTableWidgetItem(QDir(_repoRoot).relativeFilePath(file));
        pathItem->setData(Qt::UserRole, file);
        _ipFilesTable->setItem(row, 2, pathItem);
        _ipFilesTable->setItem(row, 3, new QTableWidgetItem(sample));
        ++row;
    }

    _scanIpButton->setEnabled(true);
    statusBar()->showMessage(QString("%1. %2: %3")
                                 .arg(l("Сканирование завершено", "Scan completed"))
                                 .arg(l("Файлов с IP", "Files with IP"))
                                 .arg(_ipFilesTable->rowCount()));
}

void MainWindow::applyIpChanges()
{
    const QString targetIp = _publicIpEdit->text().trimmed();
    const QHostAddress address(targetIp);
    if (address.isNull() || address.protocol() != QAbstractSocket::IPv4Protocol)
    {
        statusBar()->showMessage(l("Введите корректный IPv4", "Enter valid IPv4"));
        return;
    }

    const QRegularExpression ipv4Regex(R"((?<!\d)(?<prefix>[sS]?)(?<ip>(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d))\b)");
    int changedFiles = 0;
    int changedValues = 0;
    QStringList changedFileReports;

    for (int row = 0; row < _ipFilesTable->rowCount(); ++row)
    {
        if (!tableCheckState(_ipFilesTable->item(row, 0)))
        {
            continue;
        }

        const QString absolutePath = _ipFilesTable->item(row, 2)->data(Qt::UserRole).toString();
        QString content;
        QString encodingTag;
        bool hasUtf8Bom = false;
        if (!tryReadFile(absolutePath, content, encodingTag, hasUtf8Bom))
        {
            continue;
        }

        int localChanged = 0;
        QString replaced;
        replaced.reserve(content.size());
        int lastPos = 0;
        auto it = ipv4Regex.globalMatch(content);
        while (it.hasNext())
        {
            const QRegularExpressionMatch match = it.next();
            const int start = match.capturedStart();
            const int len = match.capturedLength();
            replaced += content.mid(lastPos, start - lastPos);

            const QString prefix = match.captured("prefix");
            const QString ipValue = match.captured("ip");
            QString replacement = match.captured(0);
            if (!(_skipLocalIpCheck->isChecked() && (ipValue == "127.0.0.1" || ipValue == "0.0.0.0")) && ipValue != targetIp)
            {
                replacement = prefix + targetIp;
                ++localChanged;
            }
            replaced += replacement;
            lastPos = start + len;
        }
        replaced += content.mid(lastPos);

        if (localChanged == 0)
        {
            continue;
        }

        if (_createBackupCheck->isChecked())
        {
            QFile::copy(absolutePath, makeBackupPath(absolutePath));
        }

        if (!writeFile(absolutePath, replaced, encodingTag, hasUtf8Bom))
        {
            continue;
        }

        changedFiles++;
        changedValues += localChanged;
        const QString relative = _ipFilesTable->item(row, 2)->text();
        changedFileReports.append(QString("%1 — %2").arg(relative).arg(localChanged));
    }

    statusBar()->showMessage(QString("%1. %2: %3, %4: %5")
                                 .arg(l("Готово", "Done"))
                                 .arg(l("Изменено файлов", "Changed files"))
                                 .arg(changedFiles)
                                 .arg(l("Замен IP", "IP replacements"))
                                 .arg(changedValues));
    if (!changedFileReports.isEmpty())
    {
        QString report;
        const int maxLines = 20;
        for (int i = 0; i < changedFileReports.size() && i < maxLines; ++i)
        {
            report += changedFileReports[i] + "\n";
        }
        if (changedFileReports.size() > maxLines)
        {
            report += QString("%1 %2 %3")
                          .arg(l("... и ещё", "... and"))
                          .arg(changedFileReports.size() - maxLines)
                          .arg(l("файлов", "files"));
        }
        QMessageBox::information(this, l("Отчёт по замене IP", "IP replacement report"), report.trimmed());
    }
}

void MainWindow::detectIpAuto()
{
    _detectIpButton->setEnabled(false);
    statusBar()->showMessage(l("Определение IP...", "Detecting IP..."));

    _localIpDetected = getLocalIpAddress();
    _externalIpDetected = getExternalIpAddress();

    _localIpLabel->setText(_localIpDetected);
    _externalIpLabel->setText(_externalIpDetected);

    if (!_externalIpDetected.isEmpty())
    {
        _useExternalIpRadio->setChecked(true);
        _publicIpEdit->setText(_externalIpDetected);
    }
    else if (!_localIpDetected.isEmpty())
    {
        _useLocalIpRadio->setChecked(true);
        _publicIpEdit->setText(_localIpDetected);
    }

    _detectIpButton->setEnabled(true);
    statusBar()->showMessage((_localIpDetected.isEmpty() && _externalIpDetected.isEmpty()) ? l("IP не найден", "IP not found") : l("IP определен", "IP detected"));
}

QString MainWindow::detectRepositoryRoot() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    while (dir.exists())
    {
        if (dir.exists("GameServer") && dir.exists("JoinServer") && dir.exists("Tools"))
        {
            return dir.absolutePath();
        }
        if (!dir.cdUp())
        {
            break;
        }
    }
    return QCoreApplication::applicationDirPath();
}

QStringList MainWindow::enumerateOdbcConfigFiles(const QString& root) const
{
    QStringList files;
    QDirIterator it(root, {"JoinServer.ini", "DataServer.ini"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        files.append(it.next());
    }
    return files;
}

QStringList MainWindow::enumerateConfigFiles(const QString& root) const
{
    QStringList files;
    static const QSet<QString> extensions = {".ini", ".xml", ".txt", ".cfg", ".json", ".dat"};
    static const QSet<QString> ignoredFolders = {".git", "bin", "obj", ".vs", "packages", "node_modules"};
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString filePath = it.next();
        const QFileInfo info(filePath);
        if (info.size() > 4 * 1024 * 1024)
        {
            continue;
        }
        if (!extensions.contains(info.suffix().prepend('.').toLower()))
        {
            continue;
        }

        bool skip = false;
        QDir parent = info.absoluteDir();
        while (parent.path().startsWith(root, Qt::CaseInsensitive))
        {
            const QString folderName = parent.dirName().toLower();
            if (ignoredFolders.contains(folderName))
            {
                skip = true;
                break;
            }
            if (parent.absolutePath().compare(root, Qt::CaseInsensitive) == 0)
            {
                break;
            }
            if (!parent.cdUp())
            {
                break;
            }
        }
        if (skip)
        {
            continue;
        }
        files.append(filePath);
    }
    return files;
}

QStringList MainWindow::enumerateIpConfigFiles(const QString& root) const
{
    QStringList result;
    const QStringList all = enumerateConfigFiles(root);
    for (const auto& file : all)
    {
        if (isTargetIpConfigFile(file))
        {
            result.append(file);
        }
    }
    return result;
}

bool MainWindow::isTargetIpConfigFile(const QString& path)
{
    const QString baseName = QFileInfo(path).completeBaseName();
    return baseName.compare("MapServerInfo", Qt::CaseInsensitive) == 0
        || baseName.compare("ServerList", Qt::CaseInsensitive) == 0;
}

QString MainWindow::readIniValue(const QString& content, const QString& key)
{
    const QRegularExpression re(QString(R"((?im)^\s*%1\s*=\s*([^\r\n;]+))").arg(QRegularExpression::escape(key)));
    const auto match = re.match(content);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString MainWindow::getLocalIpAddress()
{
    const QHostInfo host = QHostInfo::fromName(QHostInfo::localHostName());
    for (const QHostAddress& address : host.addresses())
    {
        if (address.protocol() != QAbstractSocket::IPv4Protocol)
        {
            continue;
        }
        const QString text = address.toString();
        if (text.startsWith("169.254."))
        {
            continue;
        }
        return text;
    }
    return {};
}

QString MainWindow::getExternalIpAddress()
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://api.ipify.org"));
    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QString result;
    if (reply->error() == QNetworkReply::NoError)
    {
        const QString text = QString::fromUtf8(reply->readAll()).trimmed();
        const QHostAddress addr(text);
        if (!addr.isNull() && addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            result = text;
        }
    }
    reply->deleteLater();
    return result;
}

bool MainWindow::tryReadFile(const QString& path, QString& content, QString& encodingTag, bool& hasUtf8Bom)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    const QByteArray bytes = file.readAll();
    hasUtf8Bom = false;
    encodingTag = "local8bit";

    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes[0]) == 0xEF
        && static_cast<unsigned char>(bytes[1]) == 0xBB
        && static_cast<unsigned char>(bytes[2]) == 0xBF)
    {
        hasUtf8Bom = true;
        encodingTag = "utf8";
        content = QString::fromUtf8(bytes.constData() + 3, bytes.size() - 3);
        return true;
    }

    if (bytes.size() >= 2
        && static_cast<unsigned char>(bytes[0]) == 0xFF
        && static_cast<unsigned char>(bytes[1]) == 0xFE)
    {
        encodingTag = "utf16le";
        content = QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData() + 2), (bytes.size() - 2) / 2);
        return true;
    }

    if (bytes.size() >= 2
        && static_cast<unsigned char>(bytes[0]) == 0xFE
        && static_cast<unsigned char>(bytes[1]) == 0xFF)
    {
        encodingTag = "utf16be";
        QStringDecoder decoder(QStringDecoder::Utf16BE);
        content = decoder.decode(bytes.sliced(2));
        return true;
    }

    encodingTag = "local8bit";
    content = QString::fromLocal8Bit(bytes);
    return true;
}

bool MainWindow::writeFile(const QString& path, const QString& content, const QString& encodingTag, bool hasUtf8Bom)
{
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }

    QByteArray bytes;
    if (encodingTag == "utf16le")
    {
        bytes.reserve(2 + content.size() * 2);
        bytes.append(static_cast<char>(0xFF));
        bytes.append(static_cast<char>(0xFE));
        const QByteArray payload(reinterpret_cast<const char*>(content.utf16()), content.size() * 2);
        bytes.append(payload);
    }
    else if (encodingTag == "utf16be")
    {
        QStringEncoder encoder(QStringEncoder::Utf16BE);
        bytes = encoder.encode(content);
        bytes.prepend(static_cast<char>(0xFF));
        bytes.prepend(static_cast<char>(0xFE));
    }
    else if (encodingTag == "utf8")
    {
        bytes = content.toUtf8();
        if (hasUtf8Bom)
        {
            bytes.prepend(QByteArray::fromHex("EFBBBF"));
        }
    }
    else
    {
        bytes = content.toLocal8Bit();
    }

    return out.write(bytes) == bytes.size();
}

QString MainWindow::makeBackupPath(const QString& path)
{
    const QString base = path + "_backup_iptool";
    if (!QFile::exists(base))
    {
        return base;
    }
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return base + "_" + stamp;
}

void MainWindow::applyProcessWindowVisibility(qint64 processId, bool visible) const
{
#ifdef Q_OS_WIN
    if (processId <= 0)
    {
        return;
    }
    WindowVisibilityContext context;
    context.processId = static_cast<DWORD>(processId);
    context.visible = visible;
    EnumWindows(ToggleWindowVisibilityProc, reinterpret_cast<LPARAM>(&context));
#else
    Q_UNUSED(processId);
    Q_UNUSED(visible);
#endif
}

bool MainWindow::tableCheckState(const QTableWidgetItem* item)
{
    return item && item->checkState() == Qt::Checked;
}

QTableWidgetItem* MainWindow::makeCheckItem(bool checked)
{
    auto* item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QTableWidgetItem* MainWindow::makeReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
