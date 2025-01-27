#include "JavaWizardWidget.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <sys.h>

#include "DesktopServices.h"
#include "FileSystem.h"
#include "JavaCommon.h"
#include "java/JavaChecker.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/java/InstallJavaDialog.h"
#include "ui/widgets/VersionSelectWidget.h"

#include "Application.h"
#include "BuildConfig.h"

JavaWizardWidget::JavaWizardWidget(QWidget* parent) : QWidget(parent)
{
    m_availableMemory = Sys::getSystemRam() / Sys::mebibyte;

    goodIcon = APPLICATION->getThemedIcon("status-good");
    yellowIcon = APPLICATION->getThemedIcon("status-yellow");
    badIcon = APPLICATION->getThemedIcon("status-bad");
    m_memoryTimer = new QTimer(this);
    setupUi();

    connect(m_minMemSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxValueChanged(int)));
    connect(m_maxMemSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxValueChanged(int)));
    connect(m_permGenSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxValueChanged(int)));
    connect(m_memoryTimer, &QTimer::timeout, this, &JavaWizardWidget::memoryValueChanged);
    connect(m_versionWidget, &VersionSelectWidget::selectedVersionChanged, this, &JavaWizardWidget::javaVersionSelected);
    connect(m_javaBrowseBtn, &QPushButton::clicked, this, &JavaWizardWidget::on_javaBrowseBtn_clicked);
    connect(m_javaPathTextBox, &QLineEdit::textEdited, this, &JavaWizardWidget::javaPathEdited);
    connect(m_javaStatusBtn, &QToolButton::clicked, this, &JavaWizardWidget::on_javaStatusBtn_clicked);
    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        connect(m_javaDownloadBtn, &QPushButton::clicked, this, &JavaWizardWidget::javaDownloadBtn_clicked);
    }
}

void JavaWizardWidget::setupUi()
{
    setObjectName(QStringLiteral("javaSettingsWidget"));
    m_verticalLayout = new QVBoxLayout(this);
    m_verticalLayout->setObjectName(QStringLiteral("verticalLayout"));

    m_versionWidget = new VersionSelectWidget(this);

    m_horizontalLayout = new QHBoxLayout();
    m_horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
    m_javaPathTextBox = new QLineEdit(this);
    m_javaPathTextBox->setObjectName(QStringLiteral("javaPathTextBox"));

    m_horizontalLayout->addWidget(m_javaPathTextBox);

    m_javaBrowseBtn = new QPushButton(this);
    m_javaBrowseBtn->setObjectName(QStringLiteral("javaBrowseBtn"));

    m_horizontalLayout->addWidget(m_javaBrowseBtn);

    m_javaStatusBtn = new QToolButton(this);
    m_javaStatusBtn->setIcon(yellowIcon);
    m_horizontalLayout->addWidget(m_javaStatusBtn);

    m_memoryGroupBox = new QGroupBox(this);
    m_memoryGroupBox->setObjectName(QStringLiteral("memoryGroupBox"));
    m_gridLayout_2 = new QGridLayout(m_memoryGroupBox);
    m_gridLayout_2->setObjectName(QStringLiteral("gridLayout_2"));
    m_gridLayout_2->setColumnStretch(0, 1);

    m_labelMinMem = new QLabel(m_memoryGroupBox);
    m_labelMinMem->setObjectName(QStringLiteral("labelMinMem"));
    m_gridLayout_2->addWidget(m_labelMinMem, 0, 0, 1, 1);

    m_minMemSpinBox = new QSpinBox(m_memoryGroupBox);
    m_minMemSpinBox->setObjectName(QStringLiteral("minMemSpinBox"));
    m_minMemSpinBox->setSuffix(QStringLiteral(" MiB"));
    m_minMemSpinBox->setMinimum(8);
    m_minMemSpinBox->setMaximum(1048576);
    m_minMemSpinBox->setSingleStep(128);
    m_labelMinMem->setBuddy(m_minMemSpinBox);
    m_gridLayout_2->addWidget(m_minMemSpinBox, 0, 1, 1, 1);

    m_labelMaxMem = new QLabel(m_memoryGroupBox);
    m_labelMaxMem->setObjectName(QStringLiteral("labelMaxMem"));
    m_gridLayout_2->addWidget(m_labelMaxMem, 1, 0, 1, 1);

    m_maxMemSpinBox = new QSpinBox(m_memoryGroupBox);
    m_maxMemSpinBox->setObjectName(QStringLiteral("maxMemSpinBox"));
    m_maxMemSpinBox->setSuffix(QStringLiteral(" MiB"));
    m_maxMemSpinBox->setMinimum(8);
    m_maxMemSpinBox->setMaximum(1048576);
    m_maxMemSpinBox->setSingleStep(128);
    m_labelMaxMem->setBuddy(m_maxMemSpinBox);
    m_gridLayout_2->addWidget(m_maxMemSpinBox, 1, 1, 1, 1);

    m_labelMaxMemIcon = new QLabel(m_memoryGroupBox);
    m_labelMaxMemIcon->setObjectName(QStringLiteral("labelMaxMemIcon"));
    m_gridLayout_2->addWidget(m_labelMaxMemIcon, 1, 2, 1, 1);

    m_labelPermGen = new QLabel(m_memoryGroupBox);
    m_labelPermGen->setObjectName(QStringLiteral("labelPermGen"));
    m_labelPermGen->setText(QStringLiteral("PermGen:"));
    m_gridLayout_2->addWidget(m_labelPermGen, 2, 0, 1, 1);
    m_labelPermGen->setVisible(false);

    m_permGenSpinBox = new QSpinBox(m_memoryGroupBox);
    m_permGenSpinBox->setObjectName(QStringLiteral("permGenSpinBox"));
    m_permGenSpinBox->setSuffix(QStringLiteral(" MiB"));
    m_permGenSpinBox->setMinimum(4);
    m_permGenSpinBox->setMaximum(1048576);
    m_permGenSpinBox->setSingleStep(8);
    m_gridLayout_2->addWidget(m_permGenSpinBox, 2, 1, 1, 1);
    m_permGenSpinBox->setVisible(false);

    m_verticalLayout->addWidget(m_memoryGroupBox);

    m_horizontalBtnLayout = new QHBoxLayout();
    m_horizontalBtnLayout->setObjectName(QStringLiteral("horizontalBtnLayout"));

    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        m_javaDownloadBtn = new QPushButton(tr("Download Java"), this);
        m_horizontalBtnLayout->addWidget(m_javaDownloadBtn);
    }

    m_autoJavaGroupBox = new QGroupBox(this);
    m_autoJavaGroupBox->setObjectName(QStringLiteral("autoJavaGroupBox"));
    m_veriticalJavaLayout = new QVBoxLayout(m_autoJavaGroupBox);
    m_veriticalJavaLayout->setObjectName(QStringLiteral("veriticalJavaLayout"));

    m_autodetectJavaCheckBox = new QCheckBox(m_autoJavaGroupBox);
    m_autodetectJavaCheckBox->setObjectName("autodetectJavaCheckBox");
    m_autodetectJavaCheckBox->setChecked(true);
    m_veriticalJavaLayout->addWidget(m_autodetectJavaCheckBox);

    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        m_autodownloadCheckBox = new QCheckBox(m_autoJavaGroupBox);
        m_autodownloadCheckBox->setObjectName("autodownloadCheckBox");
        m_autodownloadCheckBox->setEnabled(m_autodetectJavaCheckBox->isChecked());
        m_veriticalJavaLayout->addWidget(m_autodownloadCheckBox);
        connect(m_autodetectJavaCheckBox, &QCheckBox::stateChanged, this, [this] {
            m_autodownloadCheckBox->setEnabled(m_autodetectJavaCheckBox->isChecked());
            if (!m_autodetectJavaCheckBox->isChecked())
                m_autodownloadCheckBox->setChecked(false);
        });

        connect(m_autodownloadCheckBox, &QCheckBox::stateChanged, this, [this] {
            auto isChecked = m_autodownloadCheckBox->isChecked();
            m_versionWidget->setVisible(!isChecked);
            m_javaStatusBtn->setVisible(!isChecked);
            m_javaBrowseBtn->setVisible(!isChecked);
            m_javaPathTextBox->setVisible(!isChecked);
            m_javaDownloadBtn->setVisible(!isChecked);
            if (!isChecked) {
                m_verticalLayout->removeItem(m_verticalSpacer);
            } else {
                m_verticalLayout->addSpacerItem(m_verticalSpacer);
            }
        });
    }
    m_verticalLayout->addWidget(m_autoJavaGroupBox);

    m_verticalLayout->addLayout(m_horizontalBtnLayout);

    m_verticalLayout->addWidget(m_versionWidget);
    m_verticalLayout->addLayout(m_horizontalLayout);
    m_verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

    retranslate();
}

void JavaWizardWidget::initialize()
{
    m_versionWidget->initialize(APPLICATION->javalist().get());
    m_versionWidget->selectSearch();
    m_versionWidget->setResizeOn(2);
    auto s = APPLICATION->settings();
    // Memory
    observedMinMemory = s->get("MinMemAlloc").toInt();
    observedMaxMemory = s->get("MaxMemAlloc").toInt();
    observedPermGenMemory = s->get("PermGen").toInt();
    m_minMemSpinBox->setValue(observedMinMemory);
    m_maxMemSpinBox->setValue(observedMaxMemory);
    m_permGenSpinBox->setValue(observedPermGenMemory);
    updateThresholds();
    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        m_autodownloadCheckBox->setChecked(true);
    }
}

void JavaWizardWidget::refresh()
{
    if (BuildConfig.JAVA_DOWNLOADER_ENABLED && m_autodownloadCheckBox->isChecked()) {
        return;
    }
    if (JavaUtils::getJavaCheckPath().isEmpty()) {
        JavaCommon::javaCheckNotFound(this);
        return;
    }
    m_versionWidget->loadList();
}

JavaWizardWidget::ValidationStatus JavaWizardWidget::validate()
{
    switch (javaStatus) {
        default:
        case JavaStatus::NotSet:
            /* fallthrough */
        case JavaStatus::DoesNotExist:
            /* fallthrough */
        case JavaStatus::DoesNotStart:
            /* fallthrough */
        case JavaStatus::ReturnedInvalidData: {
            if (!(BuildConfig.JAVA_DOWNLOADER_ENABLED && m_autodownloadCheckBox->isChecked())) {  // the java will not be autodownloaded
                int button = QMessageBox::No;
                if (m_result.mojangPlatform == "32" && maxHeapSize() > 2048) {
                    button = CustomMessageBox::selectable(
                                 this, tr("32-bit Java detected"),
                                 tr("You selected a 32-bit installation of Java, but allocated more than 2048MiB as maximum memory.\n"
                                    "%1 will not be able to start Minecraft.\n"
                                    "Do you wish to proceed?"
                                    "\n\n"
                                    "You can change the Java version in the settings later.\n")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No | QMessageBox::Help, QMessageBox::NoButton)
                                 ->exec();

                } else {
                    button = CustomMessageBox::selectable(this, tr("No Java version selected"),
                                                          tr("You either didn't select a Java version or selected one that does not work.\n"
                                                             "%1 will not be able to start Minecraft.\n"
                                                             "Do you wish to proceed without a functional version of Java?"
                                                             "\n\n"
                                                             "You can change the Java version in the settings later.\n")
                                                              .arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                                          QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No | QMessageBox::Help,
                                                          QMessageBox::NoButton)
                                 ->exec();
                }
                switch (button) {
                    case QMessageBox::Yes:
                        return ValidationStatus::JavaBad;
                    case QMessageBox::Help:
                        DesktopServices::openUrl(QUrl(BuildConfig.HELP_URL.arg("java-wizard")));
                    /* fallthrough */
                    case QMessageBox::No:
                    /* fallthrough */
                    default:
                        return ValidationStatus::Bad;
                }
                if (button == QMessageBox::No) {
                    return ValidationStatus::Bad;
                }
            }
            return ValidationStatus::JavaBad;
        } break;
        case JavaStatus::Pending: {
            return ValidationStatus::Bad;
        }
        case JavaStatus::Good: {
            return ValidationStatus::AllOK;
        }
    }
}

QString JavaWizardWidget::javaPath() const
{
    return m_javaPathTextBox->text();
}

int JavaWizardWidget::maxHeapSize() const
{
    auto min = m_minMemSpinBox->value();
    auto max = m_maxMemSpinBox->value();
    if (max < min)
        max = min;
    return max;
}

int JavaWizardWidget::minHeapSize() const
{
    auto min = m_minMemSpinBox->value();
    auto max = m_maxMemSpinBox->value();
    if (min > max)
        min = max;
    return min;
}

bool JavaWizardWidget::permGenEnabled() const
{
    return m_permGenSpinBox->isVisible();
}

int JavaWizardWidget::permGenSize() const
{
    return m_permGenSpinBox->value();
}

void JavaWizardWidget::memoryValueChanged()
{
    bool actuallyChanged = false;
    unsigned int min = m_minMemSpinBox->value();
    unsigned int max = m_maxMemSpinBox->value();
    unsigned int permgen = m_permGenSpinBox->value();
    if (min != observedMinMemory) {
        observedMinMemory = min;
        actuallyChanged = true;
    }
    if (max != observedMaxMemory) {
        observedMaxMemory = max;
        actuallyChanged = true;
    }
    if (permgen != observedPermGenMemory) {
        observedPermGenMemory = permgen;
        actuallyChanged = true;
    }
    if (actuallyChanged) {
        checkJavaPathOnEdit(m_javaPathTextBox->text());
        updateThresholds();
    }
}

void JavaWizardWidget::javaVersionSelected(BaseVersion::Ptr version)
{
    auto java = std::dynamic_pointer_cast<JavaInstall>(version);
    if (!java) {
        return;
    }
    auto visible = java->id.requiresPermGen();
    m_labelPermGen->setVisible(visible);
    m_permGenSpinBox->setVisible(visible);
    m_javaPathTextBox->setText(java->path);
    checkJavaPath(java->path);
}

void JavaWizardWidget::on_javaBrowseBtn_clicked()
{
    auto filter = QString("Java (%1)").arg(JavaUtils::javaExecutable);
    auto raw_path = QFileDialog::getOpenFileName(this, tr("Find Java executable"), QString(), filter);
    if (raw_path.isEmpty()) {
        return;
    }
    auto cooked_path = FS::NormalizePath(raw_path);
    m_javaPathTextBox->setText(cooked_path);
    checkJavaPath(cooked_path);
}

void JavaWizardWidget::javaDownloadBtn_clicked()
{
    auto jdialog = new Java::InstallDialog({}, nullptr, this);
    jdialog->exec();
}

void JavaWizardWidget::on_javaStatusBtn_clicked()
{
    QString text;
    bool failed = false;
    switch (javaStatus) {
        case JavaStatus::NotSet:
            checkJavaPath(m_javaPathTextBox->text());
            return;
        case JavaStatus::DoesNotExist:
            text += QObject::tr("The specified file either doesn't exist or is not a proper executable.");
            failed = true;
            break;
        case JavaStatus::DoesNotStart: {
            text += QObject::tr("The specified Java binary didn't start properly.<br />");
            auto htmlError = m_result.errorLog;
            if (!htmlError.isEmpty()) {
                htmlError.replace('\n', "<br />");
                text += QString("<font color=\"red\">%1</font>").arg(htmlError);
            }
            failed = true;
            break;
        }
        case JavaStatus::ReturnedInvalidData: {
            text += QObject::tr("The specified Java binary returned unexpected results:<br />");
            auto htmlOut = m_result.outLog;
            if (!htmlOut.isEmpty()) {
                htmlOut.replace('\n', "<br />");
                text += QString("<font color=\"red\">%1</font>").arg(htmlOut);
            }
            failed = true;
            break;
        }
        case JavaStatus::Good:
            text += QObject::tr(
                        "Java test succeeded!<br />Platform reported: %1<br />Java version "
                        "reported: %2<br />")
                        .arg(m_result.realPlatform, m_result.javaVersion.toString());
            break;
        case JavaStatus::Pending:
            // TODO: abort here?
            return;
    }
    CustomMessageBox::selectable(this, failed ? QObject::tr("Java test failure") : QObject::tr("Java test success"), text,
                                 failed ? QMessageBox::Critical : QMessageBox::Information)
        ->show();
}

void JavaWizardWidget::setJavaStatus(JavaWizardWidget::JavaStatus status)
{
    javaStatus = status;
    switch (javaStatus) {
        case JavaStatus::Good:
            m_javaStatusBtn->setIcon(goodIcon);
            break;
        case JavaStatus::NotSet:
        case JavaStatus::Pending:
            m_javaStatusBtn->setIcon(yellowIcon);
            break;
        default:
            m_javaStatusBtn->setIcon(badIcon);
            break;
    }
}

void JavaWizardWidget::javaPathEdited(const QString& path)
{
    checkJavaPathOnEdit(path);
}

void JavaWizardWidget::checkJavaPathOnEdit(const QString& path)
{
    auto realPath = FS::ResolveExecutable(path);
    QFileInfo pathInfo(realPath);
    if (pathInfo.baseName().toLower().contains("java")) {
        checkJavaPath(path);
    } else {
        if (!m_checker) {
            setJavaStatus(JavaStatus::NotSet);
        }
    }
}

void JavaWizardWidget::checkJavaPath(const QString& path)
{
    if (m_checker) {
        queuedCheck = path;
        return;
    }
    auto realPath = FS::ResolveExecutable(path);
    if (realPath.isNull()) {
        setJavaStatus(JavaStatus::DoesNotExist);
        return;
    }
    setJavaStatus(JavaStatus::Pending);
    m_checker.reset(
        new JavaChecker(path, "", minHeapSize(), maxHeapSize(), m_permGenSpinBox->isVisible() ? m_permGenSpinBox->value() : 0, 0));
    connect(m_checker.get(), &JavaChecker::checkFinished, this, &JavaWizardWidget::checkFinished);
    m_checker->start();
}

void JavaWizardWidget::checkFinished(const JavaChecker::Result& result)
{
    m_result = result;
    switch (result.validity) {
        case JavaChecker::Result::Validity::Valid: {
            setJavaStatus(JavaStatus::Good);
            break;
        }
        case JavaChecker::Result::Validity::ReturnedInvalidData: {
            setJavaStatus(JavaStatus::ReturnedInvalidData);
            break;
        }
        case JavaChecker::Result::Validity::Errored: {
            setJavaStatus(JavaStatus::DoesNotStart);
            break;
        }
    }
    updateThresholds();
    m_checker.reset();
    if (!queuedCheck.isNull()) {
        checkJavaPath(queuedCheck);
        queuedCheck.clear();
    }
}

void JavaWizardWidget::retranslate()
{
    m_memoryGroupBox->setTitle(tr("Memory"));
    m_maxMemSpinBox->setToolTip(tr("The maximum amount of memory Minecraft is allowed to use."));
    m_labelMinMem->setText(tr("Minimum memory allocation:"));
    m_labelMaxMem->setText(tr("Maximum memory allocation:"));
    m_minMemSpinBox->setToolTip(tr("The amount of memory Minecraft is started with."));
    m_permGenSpinBox->setToolTip(tr("The amount of memory available to store loaded Java classes."));
    m_javaBrowseBtn->setText(tr("Browse"));
    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        m_autodownloadCheckBox->setText(tr("Auto-download Mojang Java"));
    }
    m_autodetectJavaCheckBox->setText(tr("Auto-detect Java version"));
    m_autoJavaGroupBox->setTitle(tr("Autodetect Java"));
}

void JavaWizardWidget::updateThresholds()
{
    QString iconName;

    if (observedMaxMemory >= m_availableMemory) {
        iconName = "status-bad";
        m_labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation exceeds your system memory capacity."));
    } else if (observedMaxMemory > (m_availableMemory * 0.9)) {
        iconName = "status-yellow";
        m_labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation approaches your system memory capacity."));
    } else if (observedMaxMemory < observedMinMemory) {
        iconName = "status-yellow";
        m_labelMaxMemIcon->setToolTip(tr("Your maximum memory allocation is smaller than the minimum value"));
    } else if (BuildConfig.JAVA_DOWNLOADER_ENABLED && m_autodownloadCheckBox->isChecked()) {
        iconName = "status-good";
        m_labelMaxMemIcon->setToolTip("");
    } else if (observedMaxMemory > 2048 && !m_result.is_64bit) {
        iconName = "status-bad";
        m_labelMaxMemIcon->setToolTip(tr("You are exceeding the maximum allocation supported by 32-bit installations of Java."));
    } else {
        iconName = "status-good";
        m_labelMaxMemIcon->setToolTip("");
    }

    {
        auto height = m_labelMaxMemIcon->fontInfo().pixelSize();
        QIcon icon = APPLICATION->getThemedIcon(iconName);
        QPixmap pix = icon.pixmap(height, height);
        m_labelMaxMemIcon->setPixmap(pix);
    }
}

bool JavaWizardWidget::autoDownloadJava() const
{
    return m_autodownloadCheckBox && m_autodownloadCheckBox->isChecked();
}

bool JavaWizardWidget::autoDetectJava() const
{
    return m_autodetectJavaCheckBox->isChecked();
}

void JavaWizardWidget::onSpinBoxValueChanged(int)
{
    m_memoryTimer->start(500);
}

JavaWizardWidget::~JavaWizardWidget()
{
    delete m_verticalSpacer;
};