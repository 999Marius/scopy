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

#ifndef ATTR_API_H
#define ATTR_API_H

#include "scopy-core_export.h"
#include <pluginbase/apiobject.h>

#include <QStringList>

namespace scopy {

namespace component {
class Attribute;
class Context;
} // namespace component

// Generic JS access to any attribute of a connected device, through the
// device-controller tree. Exposed to scripts as `attr`.
//
// Paths:
//   "attr"                          context attribute
//   "device/attr"                   device attribute
//   "device/channel_in/attr"        input channel attribute
//   "device/channel_out/attr"       output channel attribute
// "device" matches either the IIO device name or its id.
//
// Example:
//   attr.write("ip:192.168.2.1", "cf-ad9361-dds-core-lpc/altvoltage0_out/frequency", "100000")
class SCOPY_CORE_EXPORT Attr_API : public ApiObject
{
	Q_OBJECT
public:
	explicit Attr_API(QObject *parent = nullptr);
	~Attr_API();

	// Returns the value, or an empty string on failure (see the log).
	Q_INVOKABLE QString read(const QString &uri, const QString &path);
	// Blocks until the device answers. Returns false on failure.
	Q_INVOKABLE bool write(const QString &uri, const QString &path, const QString &value);
	// Lists the children of a path: "" -> devices and context attributes,
	// "device" -> channels and attributes, "device/channel_out" -> attributes.
	Q_INVOKABLE QStringList list(const QString &uri, const QString &path = "");

private:
	// Walks the path from the context down to the attribute; nullptr if any part is missing.
	component::Attribute *findAttribute(component::Context *ctx, const QString &path);
	// Returns the device, or the channel if the path names one; nullptr if missing.
	QObject *findNode(component::Context *ctx, const QStringList &parts);
};
} // namespace scopy

#endif // ATTR_API_H
