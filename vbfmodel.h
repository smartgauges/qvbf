#ifndef VBFMODEL_H
#define VBFMODEL_H

#include <QAbstractListModel>

#include "vbffile.h"

class VbfModel : public QAbstractListModel
{ 
	Q_OBJECT

	private:
		block_t empty;
		vbf_t vbf;

	signals:
		void sig_resize();

	public:
		VbfModel(QObject *parent = 0);

		enum enum_can
		{
			e_col_block = 0,
			e_col_size,
			e_col_rm,
			e_col_nums
		};

		int rowCount(const QModelIndex &parent = QModelIndex()) const;
		int columnCount(const QModelIndex &parent = QModelIndex()) const;
		QVariant data(const QModelIndex &index, int role) const;
		QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	public:
		void reset();
		int size();
		void set(const vbf_t & vbf);
		const vbf_t & get();
		bool add(const QString & fileName);
		void rm(int idx);
		const block_t & get_block(int idx);
		void update_block(int idx, uint32_t addr, const QByteArray & data);
		void update_header(struct header_t & header);
};

#endif

