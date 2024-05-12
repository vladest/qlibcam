/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * main.cpp - qcam - The libcamera GUI test application
 */

#include "qlibcameramanager.h"
#include "qlibcamera.h"
#include <signal.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QtDebug>
#include <SBarcodeFilter.h>
#include <tensorflowfilter.h>

void signalHandler([[maybe_unused]] int signal)
{
	qInfo() << "Exiting";
	qApp->quit();
}

int main(int argc, char **argv)
{
	int ret;
	struct sigaction sa = {};
	sa.sa_handler = &signalHandler;
	sigaction(SIGINT, &sa, nullptr);

    QGuiApplication app(argc, argv);

    qmlRegisterUncreatableType<QLibCameraManager>("CamerasManager", 1, 0,"CamerasManager","");
    qmlRegisterUncreatableType<QLibCamera>("CamerasManager", 1, 0,"LibCamera","");
    qmlRegisterSingletonInstance<QLibCameraManager>("CamerasManager", 1, 0, "CamerasManager",
                                                 QLibCameraManager::instance());

    SBarcodeFilter barcodeFilter;
    barcodeFilter.setActive(true);
    QLibCameraManager::instance()->addCameraFilter(&barcodeFilter);

    TensorFlowFilter tfFilter;
    tfFilter.setActive(true);
    QLibCameraManager::instance()->addCameraFilter(&tfFilter);

    QQmlApplicationEngine engine;
    const QUrl url("qrc:/main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    engine.rootContext()->setContextProperty("barcodeFilter", &barcodeFilter);
    engine.rootContext()->setContextProperty("tf2Filter", &tfFilter);
    engine.load(url);

	ret = app.exec();
    // stop cameras before QmlEngine gets destroyed
    QLibCameraManager::instance()->finishManager();
	return ret;
}
