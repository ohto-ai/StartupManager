#include "StartupManager.hh"
#include <QVBoxLayout>
#include <QDropEvent>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUrl>
#include <QMimeData>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include <QFileIconProvider>
#include <QMenu>
#include <QContextMenuEvent>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <QLabel>

StartupManager::StartupManager(QWidget* parent) : QMainWindow(parent) {
    setWindowIcon(QIcon(":/res/icon.ico"));
    setWindowTitle("Startup Manager");
    setAcceptDrops(true);
    setWindowOpacity(0.9);
    setStyleSheet(R"(
*{
  background-color: black;
  color: white;
  font: 300 20pt "Microsoft YaHei UI";
}
)");
    listView = new QListView(this);

    model = new QStandardItemModel(this);
    listView->setModel(model);
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listView, &QListView::customContextMenuRequested, this, &StartupManager::showContextMenu);

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->addWidget(listView);
    auto tipLabel = new QLabel("拖入可执行文件或脚本来开机启动", centralWidget);
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);

    setCentralWidget(centralWidget);

    startupDir = QDir(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/Startup/");

    loadStartupItems();
}

void StartupManager::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void StartupManager::dropEvent(QDropEvent* event) {
    foreach(const QUrl & url, event->mimeData()->urls()) {
        QString filePath = url.toLocalFile();
        if (filePath.endsWith(".lnk")) {
            handleShortcut(filePath);
        }
        else {
            addToStartup(filePath);
        }
    }
    loadStartupItems();
}

void StartupManager::addToStartup(const QString& filePath) {
    QString linkName = QFileInfo(filePath).baseName() + ".lnk"; // 确保以 .lnk 结尾
    QString linkPath = startupDir.absoluteFilePath(linkName);

    QString program = "powershell";
    QString command = QString("$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%1'); $s.TargetPath = '%2'; $s.Save()")
        .arg(linkPath, filePath);
    QStringList arguments = { "-command", command };

    QProcess process;
    process.start(program, arguments);
    process.waitForFinished();

    int ret = process.exitCode();
    if (ret != 0) {
        QMessageBox::critical(this, "Error", process.readAllStandardError());
    }
    else {
        QMessageBox::information(this, "Success", "Shortcut added to startup successfully.");
    }
}


void StartupManager::handleShortcut(const QString& shortcutPath) {
    QFile file(shortcutPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = file.readAll();
        file.close();

        QString target = extractValue(content, "Target");
        if (!target.isEmpty()) {
            addToStartup(target);
        }
    }
}

QString StartupManager::extractValue(const QString& content, const QString& key) {
    QRegularExpression regex(key + "=([^\\n]*)");
    QRegularExpressionMatch match = regex.match(content);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

void StartupManager::loadStartupItems() {
    if (startupDir.exists()) {
        // 过滤 .lnk 文件
        QStringList filters;
        filters << "*.lnk";
        startupDir.setNameFilters(filters);

        // 使用 QFileIconProvider 来获取图标
        QFileIconProvider iconProvider;

        // 获取所有 .lnk 文件
        QFileInfoList fileList = startupDir.entryInfoList(QDir::Files);
        foreach(const QFileInfo & fileInfo, fileList) {
            QString itemName = fileInfo.baseName();
            QIcon itemIcon = iconProvider.icon(fileInfo); // 获取图标

            // 创建模型项
            QStandardItem* item = new QStandardItem(itemIcon, itemName);
            item->setData(fileInfo.absoluteFilePath(), FilePathRole);

            // 添加项到模型
            model->appendRow(item);
        }
    }
    else {
        QMessageBox::warning(this, "Error", "Startup folder does not exist.");
    }
}

void StartupManager::showContextMenu(const QPoint& pos) {
    QModelIndex index = listView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    QString filePath = index.data(FilePathRole).toString();
    if (filePath.isEmpty()) {
        return;
    }

    showExplorerContextMenu(filePath, listView->viewport()->mapToGlobal(pos));
}

void StartupManager::showExplorerContextMenu(const QString& filePath, const QPoint& globalPos) {
    QString nativeFilePath = QDir::toNativeSeparators(filePath);
    LPCWSTR lpszFile = (LPCWSTR)nativeFilePath.utf16();
    ITEMIDLIST* pidl = NULL;
    HRESULT hr = E_FAIL;
    IShellFolder* psfFolder = NULL;
    PCUITEMID_CHILD pidlChild = NULL;
    IContextMenu* pcm = NULL;

    do {
        hr = SHParseDisplayName(lpszFile, NULL, &pidl, 0, NULL);
        if (FAILED(hr)) {
            break;
        }
        hr = SHBindToParent(pidl, IID_PPV_ARGS(&psfFolder), &pidlChild);
        if (FAILED(hr)) {
            break;
        }

        hr = psfFolder->GetUIObjectOf(NULL, 1, &pidlChild, IID_IContextMenu, NULL, (void**)&pcm);
        if (FAILED(hr)) {
            break;
        }
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) {
            break;
        }

        pcm->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);
        // 将菜单转换为Qt菜单并展示
        auto command = TrackPopupMenu(hMenu, TPM_RETURNCMD, globalPos.x(), globalPos.y(), 0, (HWND)listView->winId(), NULL);
        if (command > 0) {
            CMINVOKECOMMANDINFOEX cmi = { 0 };
            cmi.cbSize = sizeof(cmi);
            cmi.fMask = CMIC_MASK_UNICODE;
            cmi.hwnd = (HWND)listView->winId();
            cmi.lpVerb = MAKEINTRESOURCEA(command - 1);
            cmi.lpDirectory = NULL;
            cmi.lpVerbW = MAKEINTRESOURCEW(command - 1);
            cmi.lpDirectoryW = NULL;
            cmi.nShow = SW_SHOWNORMAL;
            pcm->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
        }

        DestroyMenu(hMenu);

    } while (false);
    if (pcm) {
        pcm->Release();
    }
    if (psfFolder) {
        psfFolder->Release();
    }
    if (pidl) {
        CoTaskMemFree(pidl);
    }
}