function MOD_byteAt(data, index) {
    return (index < data.length) ? (data[index] & 0xFF) : 0;
}

function MOD_wordAt(data, index) {
    return (MOD_byteAt(data, index) << 8) | MOD_byteAt(data, index + 1);
}

function MOD_dwordAt(data, index) {
    return (MOD_byteAt(data, index) * 16777216) +
        (MOD_byteAt(data, index + 1) << 16) +
        (MOD_byteAt(data, index + 2) << 8) +
        MOD_byteAt(data, index + 3);
}


function MOD_twoDigits(value) {
    return (value < 10 ? "0" : "") + value;
}

function MOD_formatLogTimestamp(year, month, day, hour, minute, second, uptime) {
    if (year > 0 && month > 0 && day > 0) {
        return MOD_twoDigits(day) + "." + MOD_twoDigits(month) + "." + year + " " +
            MOD_twoDigits(hour) + ":" + MOD_twoDigits(minute) + ":" + MOD_twoDigits(second) +
            " | " + uptime + " s";
    }
    return uptime + " s";
}

function MOD_errorName(code) {
    switch (code) {
        case 0x00: return "Illegal Function / lokale Konfiguration";
        case 0x01: return "Illegal Function";
        case 0x02: return "Illegal Data Address";
        case 0x03: return "Illegal Data Value";
        case 0x04: return "Slave Device Failure";
        case 0xE0: return "Falsche Slave-ID in Antwort";
        case 0xE1: return "Falscher Function-Code in Antwort";
        case 0xE2: return "Timeout / keine Antwort";
        case 0xE3: return "CRC-Fehler";
        case 0xE4: return "Illegal Function";
        default: return "Unbekannt";
    }
}

function MOD_functionName(code) {
    switch (code) {
        case 0x01: return "0x01 Coils";
        case 0x02: return "0x02 Discrete Inputs";
        case 0x03: return "0x03 Holding Registers";
        case 0x04: return "0x04 Input Registers";
        case 0x06: return "0x06 Write Single Register";
        case 0x10: return "0x10 Write Multiple Registers";
        default: return "0x" + ("0" + code.toString(16).toUpperCase()).slice(-2);
    }
}

function MOD_channelName(device, channel) {
    try {
        var p = device.getParameterByName("MOD_CH" + channel + "Name");
        if (p && p.value && p.value.length > 0)
            return p.value;
    } catch (e) { }
    return "";
}

function MOD_slaveName(device, slaveSelection) {
    try {
        var p = device.getParameterByName("MOD_SlaveName" + slaveSelection);
        if (p && p.value && p.value.length > 0)
            return p.value;
    } catch (e) { }
    return "";
}

function MOD_callErrorLog(device, online, progress, data) {
    if (typeof BASE_invokeFunctionPropertyWrapper === "function") {
        return BASE_invokeFunctionPropertyWrapper(161, 5, data, device, online, progress);
    }
    return online.invokeFunctionProperty(161, 5, data);
}

function MOD_setErrorLogSummary(device, status, count, active, events, visibleChannels, uptime) {
    device.getParameterByName("MOD_ErrorLogStatus").value = status;
    device.getParameterByName("MOD_ErrorLogEntries").value = String(count);
    device.getParameterByName("MOD_ErrorLogActive").value = String(active);
    device.getParameterByName("MOD_ErrorLogEvents").value = String(events);
    device.getParameterByName("MOD_ErrorLogVisibleChannels").value = String(visibleChannels);
    device.getParameterByName("MOD_ErrorLogUptime").value = String(uptime) + " s";
}

function MOD_setErrorLogEntries(device, rows) {
    // Erst ausblenden, damit beim Aktualisieren keine alten Einträge sichtbar bleiben.
    device.getParameterByName("MOD_ErrorLogDisplayCount").value = 0;

    for (var i = 0; i < 32; i++) {
        var row = i < rows.length ? rows[i] : null;
        var index = i + 1;
        device.getParameterByName("MOD_ErrorLogTime" + index).value = row ? row.time : "";
        device.getParameterByName("MOD_ErrorLogChannel" + index).value = row ? row.channel : "";
        device.getParameterByName("MOD_ErrorLogSlave" + index).value = row ? row.slave : "";
        device.getParameterByName("MOD_ErrorLogRegister" + index).value = row ? row.register : "";
        device.getParameterByName("MOD_ErrorLogFunction" + index).value = row ? row.func : "";

        var errorText = row ? row.error : "";
        if (errorText.length > 500)
            errorText = errorText.substr(0, 497) + "...";
        device.getParameterByName("MOD_ErrorLogOutput" + index).value = errorText;
    }

    device.getParameterByName("MOD_ErrorLogDisplayCount").value = Math.min(rows.length, 32);
    device.getParameterByName("MOD_ErrorLogLoaded").value = 1;
}

function MOD_errorSlaveText(device, slaveId) {
    var text = String(slaveId);
    for (var i = 1; i <= 10; i++) {
        try {
            var idParam = device.getParameterByName("MOD_BusID_Slave" + i);
            if (!idParam || Number(idParam.value) != slaveId)
                continue;
            var name = MOD_slaveName(device, i);
            if (name.length > 0)
                text += " | " + name;
            break;
        } catch (e) { }
    }
    return text;
}

function MOD_readErrorLogSummary(device, online, progress) {
    var summary = MOD_callErrorLog(device, online, progress, [1]);
    if (!summary || summary.length < 10 || summary[0] != 0)
        throw new Error("Modbus: Keine gültige Antwort vom Gerät beim Lesen der Fehlerlog-Zusammenfassung.");

    return {
        count: MOD_byteAt(summary, 1),
        active: MOD_byteAt(summary, 2),
        events: MOD_wordAt(summary, 3),
        visibleChannels: MOD_byteAt(summary, 5),
        uptime: MOD_dwordAt(summary, 6)
    };
}

function MOD_readErrorLog(device, online, progress, context) {
    progress.setText("Modbus: Fehlerlog wird online ausgelesen...");
    progress.setProgress(1);

    online.connect();
    try {
        var summary = MOD_readErrorLogSummary(device, online, progress);
        var rows = [];

        for (var i = 0; i < summary.count; i++) {
            progress.setText("Modbus: Fehlerlog Eintrag " + (i + 1) + "/" + summary.count + "...");
            progress.setProgress(10 + Math.floor((i * 80) / Math.max(1, summary.count)));

            var resp = MOD_callErrorLog(device, online, progress, [2, i]);
            if (!resp || resp.length < 23 || resp[0] != 0) {
                rows.push({ time: "-", channel: "-", slave: "-", register: "-", func: "-", error: "Ungültige Antwort" });
                continue;
            }

            var seq = MOD_byteAt(resp, 1);
            var channel = MOD_byteAt(resp, 2);
            var slaveId = MOD_byteAt(resp, 3);
            var err = MOD_byteAt(resp, 4);
            var fn = MOD_byteAt(resp, 5);
            var dpt = MOD_byteAt(resp, 6);
            var reg = MOD_wordAt(resp, 7);
            var repeats = MOD_wordAt(resp, 9);
            var ts = MOD_dwordAt(resp, 11);
            var flags = MOD_byteAt(resp, 15);
            var year = MOD_wordAt(resp, 16);
            var month = MOD_byteAt(resp, 18);
            var day = MOD_byteAt(resp, 19);
            var hour = MOD_byteAt(resp, 20);
            var minute = MOD_byteAt(resp, 21);
            var second = MOD_byteAt(resp, 22);
            var recovery = (flags & 0x01) != 0;
            var chName = MOD_channelName(device, channel);
            var timestamp = MOD_formatLogTimestamp(year, month, day, hour, minute, second, ts);

            var channelText = String(channel);
            if (chName.length > 0)
                channelText += " | " + chName;
            channelText += " | DPT" + dpt;

            var slaveText = MOD_errorSlaveText(device, slaveId);
            var registerText = String(reg);
            var functionText = MOD_functionShortName(fn) + " | " + MOD_functionName(fn).replace(/^0x[0-9A-Fa-f]+\s*-?\s*/, "");
            var errorText;
            if (recovery) {
                errorText = "OK | wieder erreichbar";
            } else {
                errorText = "0x" + ("0" + err.toString(16).toUpperCase()).slice(-2) + " | " + MOD_errorName(err);
            }
            if (repeats > 1)
                errorText += " | x" + repeats;

            rows.push({
                time: timestamp,
                channel: channelText,
                slave: slaveText,
                register: registerText,
                func: functionText,
                error: errorText
            });
        }

        MOD_setErrorLogSummary(
            device,
            "Fehlerlog gelesen.",
            summary.count,
            summary.active,
            summary.events,
            summary.visibleChannels,
            summary.uptime
        );
        MOD_setErrorLogEntries(device, rows);
    } finally {
        online.disconnect();
    }

    progress.setProgress(100);
    progress.setText("Modbus: Fehlerlog gelesen.");
}

function MOD_clearErrorLog(device, online, progress, context) {
    progress.setText("Modbus: Fehlerlog wird online gelöscht...");
    progress.setProgress(1);

    online.connect();
    try {
        var resp = MOD_callErrorLog(device, online, progress, [3]);
        if (!resp || resp.length < 1 || resp[0] != 0)
            throw new Error("Modbus: Fehlerlog konnte nicht gelöscht werden.");

        var summary = MOD_readErrorLogSummary(device, online, progress);
        MOD_setErrorLogSummary(
            device,
            "Fehlerlog im Gerät gelöscht.",
            summary.count,
            summary.active,
            summary.events,
            summary.visibleChannels,
            summary.uptime
        );
        MOD_setErrorLogEntries(device, []);
    } finally {
        online.disconnect();
    }

    progress.setProgress(100);
    progress.setText("Modbus: Fehlerlog gelöscht.");
}


function MOD_readAscii(data, index, length) {
    var text = "";
    for (var i = 0; i < length && (index + i) < data.length; i++) {
        var c = MOD_byteAt(data, index + i);
        if (c == 0)
            break;
        text += String.fromCharCode(c);
    }
    return text;
}

function MOD_compactNumberText(text) {
    // ETS-Oberfläche deutsch darstellen, ohne unnötige Nachkommastellen.
    if (!text)
        return text;
    var match = /^(-?\d+)\.(\d+)(.*)$/.exec(text);
    if (!match)
        return text;
    var decimals = match[2].replace(/0+$/, "");
    if (decimals.length == 0)
        return match[1] + match[3];
    return match[1] + "," + decimals + match[3];
}

function MOD_callRegisterTable(device, online, progress, data) {
    return MOD_callErrorLog(device, online, progress, data);
}

function MOD_functionShortName(code) {
    return "FC" + ("0" + code.toString(16).toUpperCase()).slice(-2);
}

function MOD_setRegisterTableRow(device, row, channelText, slaveText, registerText, valueText) {
    device.getParameterByName("MOD_RegisterTableChannel" + row).value = channelText;
    device.getParameterByName("MOD_RegisterTableSlave" + row).value = slaveText;
    device.getParameterByName("MOD_RegisterTableRegister" + row).value = registerText;
    device.getParameterByName("MOD_RegisterTableValue" + row).value = valueText;
}

function MOD_clearRegisterTable(device) {
    // Count/Loaded steuern die Sichtbarkeit. Alte, versteckte Zeilen müssen daher
    // nicht einzeln geleert werden; das spart hunderte ETS-Parameterzugriffe.
    device.getParameterByName("MOD_RegisterTableLoaded").value = 0;
    device.getParameterByName("MOD_RegisterTableCount").value = 0;
}

function MOD_readRegisterTable(device, online, progress, context) {
    progress.setText("Modbus: Registerwerte werden online ausgelesen...");
    progress.setProgress(1);

    MOD_clearRegisterTable(device);
    device.getParameterByName("MOD_RegisterTableStatus").value = "Registerwerte werden gelesen...";

    online.connect();
    try {
        var summary = MOD_callRegisterTable(device, online, progress, [4]);
        if (!summary || summary.length < 7 || summary[0] != 0)
            throw new Error("Modbus: Keine gültige Antwort vom Gerät beim Lesen der Registerübersicht.");

        var count = MOD_byteAt(summary, 1);
        var uptime = MOD_dwordAt(summary, 3);
        if (count > 200)
            count = 200;

        for (var i = 0; i < count; i++) {
            progress.setText("Modbus: Registerwert " + (i + 1) + "/" + count + "...");
            progress.setProgress(5 + Math.floor((i * 90) / Math.max(1, count)));

            var resp = MOD_callRegisterTable(device, online, progress, [5, i]);
            if (!resp || resp.length < 13 || resp[0] != 0) {
                MOD_setRegisterTableRow(device, i + 1, (i + 1) + " | <ungültig>", "-", "-", "Keine Geräteantwort");
                continue;
            }

            var channel = MOD_byteAt(resp, 1);
            var slaveSelection = MOD_byteAt(resp, 2);
            var slaveId = MOD_byteAt(resp, 3);
            var fn = MOD_byteAt(resp, 4);
            var dpt = MOD_byteAt(resp, 5);
            var direction = MOD_byteAt(resp, 6);
            var registerAddress = MOD_wordAt(resp, 7);
            var flags = MOD_byteAt(resp, 9);
            var errorCode = MOD_byteAt(resp, 10);
            var rawLength = MOD_byteAt(resp, 11);
            var knxLength = MOD_byteAt(resp, 12);
            var rawText = MOD_compactNumberText(MOD_readAscii(resp, 13, rawLength));
            var knxText = MOD_compactNumberText(MOD_readAscii(resp, 13 + rawLength, knxLength));

            var channelName = MOD_channelName(device, channel);
            var channelText = String(channel);
            if (channelName.length > 0)
                channelText += " | " + channelName;
            channelText += " | DPT" + dpt + " | " + (direction ? "M->K" : "K->M");

            var slaveText = String(slaveId);
            var slaveName = MOD_slaveName(device, slaveSelection);
            if (slaveName.length > 0)
                slaveText += " | " + slaveName;

            var registerText = String(registerAddress) + " | " + MOD_functionShortName(fn);

            if ((flags & 0x01) == 0 || rawText.length == 0)
                rawText = "-";
            if ((flags & 0x04) == 0 || knxText.length == 0)
                knxText = "-";

            // Nur eine ETS-Wertspalte verwenden: so bleiben wir im bereits belegten
            // Parameterbereich der ersten Registertabelle und benötigen keine weiteren IDs.
            // M->K: Modbus-Rohwert (KNX-Wert), K->M: KNX-Wert (geschriebener Rohwert).
            var valueText;
            if (direction) {
                if (rawText == knxText)
                    valueText = rawText;
                else if (knxText == "-")
                    valueText = rawText;
                else if (rawText == "-")
                    valueText = "- (" + knxText + ")";
                else
                    valueText = rawText + " (" + knxText + ")";
            } else {
                if (rawText == knxText)
                    valueText = knxText;
                else if (rawText == "-")
                    valueText = knxText;
                else if (knxText == "-")
                    valueText = "- (" + rawText + ")";
                else
                    valueText = knxText + " (" + rawText + ")";
            }

            if ((flags & 0x02) != 0)
                valueText += " | ERR 0x" + ("0" + errorCode.toString(16).toUpperCase()).slice(-2);

            MOD_setRegisterTableRow(device, i + 1, channelText, slaveText, registerText, valueText);
        }

        device.getParameterByName("MOD_RegisterTableStatus").value =
            count + " aktive Register | Laufzeit: " + uptime + " s";
        device.getParameterByName("MOD_RegisterTableCount").value = count;
        device.getParameterByName("MOD_RegisterTableLoaded").value = 1;
    } finally {
        online.disconnect();
    }

    progress.setProgress(100);
    progress.setText("Modbus: Registerwerte gelesen.");
}
