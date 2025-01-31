#include "JavaWizardPage.h"
#include "Application.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <sys.h>

#include "JavaCommon.h"

#include "ui/widgets/JavaWizardWidget.h"
#include "ui/widgets/VersionSelectWidget.h"

JavaWizardPage::JavaWizardPage(QWidget* parent) : BaseWizardPage(parent)
{
    setupUi();
}

void JavaWizardPage::setupUi()
{
    setObjectName(QStringLiteral("javaPage"));
    QVBoxLayout* layout = new QVBoxLayout(this);

    m_java_widget = new JavaWizardWidget(this);
    layout->addWidget(m_java_widget);
    setLayout(layout);

    retranslate();
}

void JavaWizardPage::refresh()
{
    m_java_widget->refresh();
}

void JavaWizardPage::initializePage()
{
    m_java_widget->initialize();
}

bool JavaWizardPage::wantsRefreshButton()
{
    return true;
}

bool JavaWizardPage::validatePage()
{
    auto settings = APPLICATION->settings();
    auto result = m_java_widget->validate();
    settings->set("AutomaticJavaSwitch", m_java_widget->autoDetectJava());
    settings->set("AutomaticJavaDownload", m_java_widget->autoDownloadJava());
    settings->set("UserAskedAboutAutomaticJavaDownload", true);
    switch (result) {
        default:
        case JavaWizardWidget::ValidationStatus::Bad: {
            return false;
        }
        case JavaWizardWidget::ValidationStatus::AllOK: {
            settings->set("JavaPath", m_java_widget->javaPath());
        } /* fallthrough */
        case JavaWizardWidget::ValidationStatus::JavaBad: {
            // Memory
            auto s = APPLICATION->settings();
            s->set("MinMemAlloc", m_java_widget->minHeapSize());
            s->set("MaxMemAlloc", m_java_widget->maxHeapSize());
            if (m_java_widget->permGenEnabled()) {
                s->set("PermGen", m_java_widget->permGenSize());
            } else {
                s->reset("PermGen");
            }
            return true;
        }
    }
}

void JavaWizardPage::retranslate()
{
    setTitle(tr("Java"));
    setSubTitle(
        tr("Please select how much memory to allocate to instances and if Prism Launcher should manage Java automatically or manually."));
    m_java_widget->retranslate();
}
