/*
 * Copyright (c) 2026 Analog Devices Inc.
 *
 * This file is part of Scopy
 * (see https://www.github.com/analogdevicesinc/scopy).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "attr_api.h"

#include <component/attribute.h>
#include <component/attributereader.h>
#include <component/attributewriter.h>
#include <component/channel.h>
#include <component/context.h>
#include <component/controller.h>
#include <component/device.h>
#include <component/navigation.h>

#include <qcoro/qcorotask.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(CAT_ATTR_API, "Attr_API")

using namespace scopy;

Attr_API::Attr_API(QObject *parent)
	: ApiObject(parent)
{}

Attr_API::~Attr_API() {}

QString Attr_API::read(const QString &uri, const QString &path)
{
	component::ContextHandle ctx = component::Controller::context(uri);
	if(!ctx) {
		qWarning(CAT_ATTR_API) << "No connected context for" << uri;
		return QString();
	}

	component::Attribute *a = findAttribute(ctx.get(), path);
	if(!a || !a->readCapability()) {
		qWarning(CAT_ATTR_API) << "Attribute not found or not readable:" << path;
		return QString();
	}

	if(!QCoro::waitFor(a->readCapability()->readAsync())) {
		qWarning(CAT_ATTR_API) << "Cannot read" << path;
		return QString();
	}
	return a->cachedValue().trimmed();
}

bool Attr_API::write(const QString &uri, const QString &path, const QString &value)
{
	component::ContextHandle ctx = component::Controller::context(uri);
	if(!ctx) {
		qWarning(CAT_ATTR_API) << "No connected context for" << uri;
		return false;
	}

	component::Attribute *a = findAttribute(ctx.get(), path);
	if(!a) {
		qWarning(CAT_ATTR_API) << "Attribute not found:" << path;
		return false;
	}
	component::AttributeWriter *writer = a->writeCapability();
	if(!writer) {
		qWarning(CAT_ATTR_API) << "Attribute" << path << "is read-only";
		return false;
	}

	if(!QCoro::waitFor(writer->writeAsync(value))) {
		qWarning(CAT_ATTR_API) << "Cannot write" << value << "to" << path;
		return false;
	}
	return true;
}

QStringList Attr_API::list(const QString &uri, const QString &path)
{
	QStringList result;
	component::ContextHandle ctx = component::Controller::context(uri);
	if(!ctx) {
		qWarning(CAT_ATTR_API) << "No connected context for" << uri;
		return result;
	}

	const QStringList parts = path.split('/', Qt::SkipEmptyParts);
	QObject *node = parts.isEmpty() ? ctx.get() : findNode(ctx.get(), parts);
	if(!node) {
		qWarning(CAT_ATTR_API) << "Path not found:" << path;
		return result;
	}

	for(component::Device *dev : node->findChildren<component::Device *>(QString(), Qt::FindDirectChildrenOnly)) {
		result.append(dev->name().isEmpty() ? dev->id() : dev->name());
	}
	for(component::Channel *ch : node->findChildren<component::Channel *>(QString(), Qt::FindDirectChildrenOnly)) {
		result.append(ch->id() + (ch->isOutput() ? "_out" : "_in"));
	}
	for(component::Attribute *a : node->findChildren<component::Attribute *>(QString(), Qt::FindDirectChildrenOnly)) {
		result.append(a->name());
	}
	return result;
}

component::Attribute *Attr_API::findAttribute(component::Context *ctx, const QString &path)
{
	const QStringList parts = path.split('/', Qt::SkipEmptyParts);
	if(parts.isEmpty() || parts.size() > 3) {
		return nullptr;
	}
	if(parts.size() == 1) {
		return component::attributeByName(ctx, parts.first());
	}

	QObject *node = findNode(ctx, parts.mid(0, parts.size() - 1));
	return component::attributeByName(node, parts.last());
}

QObject *Attr_API::findNode(component::Context *ctx, const QStringList &parts)
{
	if(parts.isEmpty() || parts.size() > 2) {
		return nullptr;
	}

	component::Device *device = nullptr;
	for(component::Device *dev : ctx->findChildren<component::Device *>(QString(), Qt::FindDirectChildrenOnly)) {
		if(dev->name() == parts[0] || dev->id() == parts[0]) {
			device = dev;
			break;
		}
	}
	if(!device || parts.size() == 1) {
		return device;
	}

	// "altvoltage0_out" -> id "altvoltage0", output
	const QString &chPart = parts[1];
	if(chPart.endsWith("_out")) {
		return component::channelById(device, chPart.chopped(4), true);
	}
	if(chPart.endsWith("_in")) {
		return component::channelById(device, chPart.chopped(3), false);
	}
	qWarning(CAT_ATTR_API) << "Channel" << chPart << "needs an _in or _out suffix";
	return nullptr;
}

#include "moc_attr_api.cpp"
