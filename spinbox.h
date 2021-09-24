#ifndef SPINBOX_H
#define SPINBOX_H

#include <QString>
#include <QDoubleSpinBox>
#include <QValidator>
#include <inttypes.h>

class SpinBox : public QDoubleSpinBox
{

	Q_OBJECT
	public:
		SpinBox(QWidget *parent = 0) : QDoubleSpinBox(parent)
		{
		}

		void setDisplayIntegerBase(int)
		{
		}

	protected:
		QString textFromValue(double value) const
		{
			uint32_t v = value;
			return QString::number(v, 16).toUpper();
		}

		double valueFromText(const QString &text) const
		{
			//bool ok;
			return text.toUInt(0, 16);//text.toLongLong(&ok, 16);
		}

		virtual QValidator::State validate(QString &text, int &) const
		{
			bool okay;
			text.toUInt(&okay, 16);
			if (!okay)
				return QValidator::Invalid;

			QValidator::State state = QValidator::Acceptable;

			return state;
		}
};

#endif

