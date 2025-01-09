#include "NewsDialog.h"
#include "DesktopServices.h"
#include "ui_NewsDialog.h"

NewsDialog::NewsDialog(QList<NewsEntryPtr> entries, QWidget* parent) : QDialog(parent), ui(new Ui::NewsDialog())
{
    ui->setupUi(this);

    for (auto entry : entries) {
        ui->articleListWidget->addItem(entry->title);
        m_entries.insert(entry->title, entry);
    }

    connect(ui->articleListWidget, &QListWidget::currentTextChanged, this, &NewsDialog::selectedArticleChanged);
    connect(ui->toggleListButton, &QPushButton::clicked, this, &NewsDialog::toggleArticleList);

    m_article_list_hidden = ui->articleListWidget->isHidden();

    auto first_item = ui->articleListWidget->item(0);
    first_item->setSelected(true);

    auto article_entry = m_entries.constFind(first_item->text()).value();
    ui->articleTitleLabel->setText(QString("<a href='%1'>%2</a>").arg(article_entry->link, first_item->text()));

    ui->currentArticleContentBrowser->setText(article_entry->content);
    ui->currentArticleContentBrowser->flush();
    if (DesktopServices::isGameScope()) {
        showFullScreen();
        setFixedSize(this->width(), this->height());
    }
}

NewsDialog::~NewsDialog()
{
    delete ui;
}

void NewsDialog::selectedArticleChanged(const QString& new_title)
{
    auto article_entry = m_entries.constFind(new_title).value();

    ui->articleTitleLabel->setText(QString("<a href='%1'>%2</a>").arg(article_entry->link, new_title));

    ui->currentArticleContentBrowser->setText(article_entry->content);
    ui->currentArticleContentBrowser->flush();
}

void NewsDialog::toggleArticleList()
{
    m_article_list_hidden = !m_article_list_hidden;

    ui->articleListWidget->setHidden(m_article_list_hidden);

    if (m_article_list_hidden)
        ui->toggleListButton->setText(tr("Show article list"));
    else
        ui->toggleListButton->setText(tr("Hide article list"));
}
