#include <QIcon>

#include "vbfmodel.h"

VbfModel::VbfModel(QObject *parent) : QAbstractListModel(parent)
{
	reset();
}

void VbfModel::reset()
{
	beginResetModel();
	vbf.reset();
	endResetModel();
}

int VbfModel::columnCount(const QModelIndex &) const
{
	return e_col_nums;
}

int VbfModel::rowCount(const QModelIndex &) const
{
	return vbf.blocks.size() + 1/*header*/;
}

QVariant VbfModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole)
		return QVariant();

	if (orientation == Qt::Horizontal) {

		switch (section) {

			case e_col_block:
				return tr("Blocks");
			case e_col_size:
				return tr("Bytes");
		}
	}

	return QVariant();
}

QVariant VbfModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (index.row() >= (vbf.blocks.size() + 1/*header*/))
		return QVariant();

	if (index.column() >= e_col_nums)
		return QVariant();

	if (role == Qt::DisplayRole) {

		switch (index.column()) {

			case e_col_block:
				if (!index.row()) {

					return QString("header");
				}
				else {

					return QString("block %1").arg(index.row());
				}

			case e_col_size:
				if (!index.row()) {

					return vbf.header.data.size();
				}
				else {

					const block_t & block = vbf.blocks[index.row() - 1/*header*/];
					if (vbf.header.data_format_identifier_exist && vbf.header.data_format_identifier == 0x10)
						return QString("%1(%2)").arg(block.len).arg(block.data.size());
					else
						return QString("%1").arg(block.len);
				}
		}
	}
	else if (role == Qt::DecorationRole) {

		if ((index.column() == e_col_rm) && index.row()) {

			QVariant v;
			v.setValue(QIcon(":/images/del.png"));

			return v;
		}
	}

	return QVariant();
}

void VbfModel::set(const vbf_t & _vbf)
{
	beginResetModel();
	vbf = _vbf;
	endResetModel();
}

const vbf_t & VbfModel::get()
{
	return vbf;
}

bool VbfModel::add(const QString & fileName)
{
	beginResetModel();
	bool r = vbf_add(fileName, vbf);
	endResetModel();

	return r;
}

void VbfModel::rm(int idx)
{
	if (idx > vbf.blocks.size() || idx < 1)
		return;

	beginResetModel();
	vbf.blocks.remove(idx - 1/*header*/);
	endResetModel();
}

int VbfModel::size()
{
	return vbf.blocks.size();
}

const block_t & VbfModel::get_block(int idx)
{
	if (idx > vbf.blocks.size())
		return empty;

	return vbf.blocks[idx];
}

void VbfModel::update_block(int idx, uint32_t addr, const QByteArray & data)
{
	if (idx > vbf.blocks.size())
		return;

	block_t & block = vbf.blocks[idx];

	block.addr = addr;
	block.data = data;
	block.len = block.data.size();
}

void VbfModel::update_header(struct header_t & header)
{
	vbf.header = header;

	vbf_update_header(vbf);
}

