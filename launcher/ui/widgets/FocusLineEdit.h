#pragma once

#include <QLineEdit>

class FocusLineEdit : public QLineEdit {
    Q_OBJECT
   public:
    FocusLineEdit(QWidget* parent);
    virtual ~FocusLineEdit() {}

   protected:
    void focusInEvent(QFocusEvent* e);
    void mousePressEvent(QMouseEvent* me);

    bool _selectOnMousePress;
};
