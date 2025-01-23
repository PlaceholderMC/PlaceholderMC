#pragma once

#include <QRegularExpression>
#include "IPathMatcher.h"

class RegexpMatcher : public IPathMatcher {
   public:
    virtual ~RegexpMatcher() {}
    RegexpMatcher(const QString& regexp)
    {
        m_regexp.setPattern(regexp);
        m_onlyFilenamePart = !regexp.contains('/');
    }

    RegexpMatcher& caseSensitive(bool cs = true)
    {
        if (cs) {
            m_regexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        } else {
            m_regexp.setPatternOptions(QRegularExpression::NoPatternOption);
        }
        return *this;
    }

    virtual bool matches(const QString& string) const override
    {
        if (m_onlyFilenamePart) {
            auto slash = string.lastIndexOf('/');
            if (slash != -1) {
                auto part = string.mid(slash + 1);
                return m_regexp.match(part).hasMatch();
            }
        }
        return m_regexp.match(string).hasMatch();
    }
    QRegularExpression m_regexp;
    bool m_onlyFilenamePart = false;
};
