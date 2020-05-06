/*
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/&gt;.
 *
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusObjectPath>
#include <QDBusReply>
#include <QSystemTrayIcon>
#include <QTimer>

#include "clickLabel.h"
#include "MacroFile.h"

//typedef void(*GAsyncReadyCallback) (GDrive *source_object,GAsyncResult *res,gpointer user_data);

void frobnitz_result_func(GDrive *source_object,GAsyncResult *res,MainWindow *p_this)
{
    gboolean success =  FALSE;
    GError *err = NULL;
    success = g_drive_eject_with_operation_finish (source_object, res, &err);

    if (!err)
    {
      qDebug()<<"Hurray!";
      findGDriveList()->removeOne(source_object);
      qDebug()<<findGDriveList()->size()<<"+-+-+-+-+-+-+-+-+-";
      p_this->m_eject = new ejectInterface(p_this,g_drive_get_name(source_object));
      p_this->m_eject->show();
    }

    else
    {
      qDebug()<<"oh no"<<err->message<<err->code;
    }

    if(findGDriveList()->size() == 0 || findGVolumeList()->size() == 0)
    {
        p_this->m_systray->hide();
    }

}

void frobnitz_result_func_drive(GDrive *source_object,GAsyncResult *res,MainWindow *p_this)
{
    gboolean success =  FALSE;
    GError *err = NULL;
    success = g_drive_start_finish (source_object, res, &err);
    if(!err)
    {
        qDebug()<<"drive start sucess";
    }
    else
    {
        qDebug()<<"drive start failed"<<"-------------------"<<err->message<<err->code;
    }
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    //框架的样式设置
    int hign = 0;
    interfaceHideTime = new QTimer(this);
    QPainterPath path;
    auto rect = this->rect();
    rect.adjust(0,0,-0,-0);
    path.addRoundedRect(rect, 6, 6);
    setProperty("blurRegion", QRegion(path.toFillPolygon().toPolygon()));
    this->setStyleSheet("QWidget{border:none;border-radius:6px;}");
    this->setAttribute(Qt::WA_TranslucentBackground);//设置窗口背景透明
    ui->centralWidget->setStyleSheet(
                "#centralWidget{"
                "width:280px;"
                "height:192px;"
                "background:rgba(19,19,20,0.95);"
                "border:1px solid rgba(255, 255, 255, 0.05);"
                "opacity:0.75;"

                "border-radius:6px;"
                "box-shadow:0px 2px 6px 0px rgba(0, 0, 0, 0.2);"
//                "margin:0px;"
//                "border-width:0px;"
//                "padding:0px;"
                "}"
                );


    iconSystray = QIcon::fromTheme("drive-removable-media");
    vboxlayout = new QVBoxLayout();
    //hboxlayout = new QHBoxLayout();

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
    this->setWindowOpacity(0.95);
    //this->resize( QSize( 280, 192 ));
    m_systray = new QSystemTrayIcon;
    //show();
    //m_systray->setIcon(QIcon("/usr/share/icons/ukui-icon-theme-default/22x22/devices/drive-removable-media.png"));
    m_systray->setIcon(iconSystray);
    m_systray->setToolTip(tr("usb management tool"));
    screen = qApp->primaryScreen();
    getDeviceInfo();
    connect(m_systray, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
    connect(this,&MainWindow::convertShowWindow,this,&MainWindow::onConvertShowWindow);
    ui->centralWidget->setLayout(vboxlayout);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::getDeviceInfo()
{

    GVolumeMonitor *g_volume_monitor = g_volume_monitor_get();
    g_signal_connect (g_volume_monitor, "drive-connected", G_CALLBACK (drive_connected_callback), this);
    g_signal_connect (g_volume_monitor, "drive-disconnected", G_CALLBACK (drive_disconnected_callback), this);
    g_signal_connect (g_volume_monitor, "volume-added", G_CALLBACK (volume_added_callback), this);
    g_signal_connect (g_volume_monitor, "volume-removed", G_CALLBACK (volume_removed_callback), NULL);
//about drive
    GList *current_drive_list = g_volume_monitor_get_connected_drives(g_volume_monitor);
    while(current_drive_list)
    {
        qDebug()<<"if gdrive can go";
        GDrive *gdrive = (GDrive *)current_drive_list->data;
        if(g_drive_can_eject(gdrive))
        {
            if(g_volume_can_eject((GVolume *)g_list_nth_data(g_drive_get_volumes(gdrive),0)))
            {
                *findGDriveList()<<gdrive;
                qDebug()<<"findGDriveList()->size():"<<findGDriveList()->size();
            }
        }
        current_drive_list = current_drive_list->next;
        qDebug()<<"gdrive can go";
        //if(g_drive_can_eject)
    }
//about volume
    GList *current_volume_list = g_volume_monitor_get_volumes(g_volume_monitor);
    qDebug()<<"how many volumes do i have?"<<g_list_length(current_volume_list);
    while(current_volume_list)
    {
        qDebug()<<"if gvolume can go";
        GVolume *gvolume = (GVolume *)current_volume_list->data;
        if(g_volume_can_eject(gvolume) || g_drive_can_eject(g_volume_get_drive(gvolume)))
        {
            *findGVolumeList()<<gvolume;
            g_volume_mount(gvolume,
                           G_MOUNT_MOUNT_NONE,
                           NULL,
                           NULL,
                           NULL,
                           NULL);
        }
        current_volume_list = current_volume_list->next;
        qDebug()<<"gvolume can go";
    }

    qDebug()<<"findGDriveList.size:"<<findGDriveList()->size()<<"11111111111111";
    if(findGVolumeList()->size() >= 1 || findGDriveList()->size() >= 1)
    {
        m_systray->show();
    }
    if(findGDriveList()->size() == 0)
    {
        m_systray->hide();
    }
}

void MainWindow::onConvertShowWindow()
{
    num = 0;
    MainWindowShow();
}

void MainWindow::drive_connected_callback(GVolumeMonitor *monitor, GDrive *drive, MainWindow *p_this)
{
    *findGDriveList()<<drive;
    qDebug()<<findGDriveList()->size()<<"findGDriveList";
    qDebug()<<g_list_length(g_drive_get_volumes(drive))<<"g_list_length";
    if(findGDriveList()->size() <= 1)
    {
        p_this->m_systray->show();
    }
}

void MainWindow::drive_disconnected_callback (GVolumeMonitor *, GDrive *drive, MainWindow *p_this)
{
    qDebug()<<"drive disconnected";
    char *drive_name = g_drive_get_name(drive);
    qDebug()<<"name: "<<drive_name;
    findGDriveList()->removeOne(drive);
    p_this->hide();
    if(findGDriveList()->size() == 0)
    {
        p_this->m_systray->hide();
    }
}

void MainWindow::volume_added_callback(GVolumeMonitor *monitor, GVolume *volume, MainWindow *p_this)
{
    qDebug()<<"volume is to be added";
    g_volume_mount(volume,
                   G_MOUNT_MOUNT_NONE,
                   NULL,
                   NULL,
                   GAsyncReadyCallback(frobnitz_result_func_volume),
                   p_this);
}

void MainWindow::volume_removed_callback(GVolumeMonitor *monitor, GVolume *volume, gpointer)
{
    findGVolumeList()->removeOne(volume);
    qDebug()<<"findGVolumeList:"<<findGVolumeList()->size();
}

void MainWindow::frobnitz_result_func_volume(GVolume *source_object,GAsyncResult *res,MainWindow *p_this)
{

    qDebug()<<"p_this:"<<p_this;
    gboolean success =  FALSE;
    GError *err = NULL;
    success = g_volume_mount_finish (source_object, res, &err);
    qDebug()<<success<<"if i have been successful";
    qDebug()<<"g_list_length(g_drive_get_volumes(g_volume_get_drive(source_object)))"<<g_list_length(g_drive_get_volumes(g_volume_get_drive(source_object)));
    if(!err)
    {
        qDebug()<<"ysysyssyeysysysysyssyeyseyseyseys";
        *findGVolumeList()<<source_object;
        p_this->num++;
        qDebug()<<findGVolumeList()->size()<<"dfsdsdadadadsdasdadadada";
        qDebug()<<p_this->num<<"---------------------------";
        if (p_this->num >= g_list_length(g_drive_get_volumes(g_volume_get_drive(source_object))))
        {
            qDebug()<<g_list_length(g_drive_get_volumes(g_volume_get_drive(source_object)))<<"-=-=-=_+_+_+_+_+_+_+";
            Q_EMIT p_this->convertShowWindow();
        }
    }
    else
    {
        qDebug()<<"oh no no no no no sorry i'm wrong"<<err->message<<err->code;
//        g_volume_mount(source_object,
//                       G_MOUNT_MOUNT_NONE,
//                       NULL,
//                       NULL,
//                       GAsyncReadyCallback(frobnitz_result_func_volume),
//                       NULL);
    }



}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{

//    if(this->interfaceHideTime != NULL)
//    {
//        delete this->interfaceHideTime;
//    }
    //int hign = 200;
    if(ui->centralWidget != NULL)
    {
        disconnect(interfaceHideTime, SIGNAL(timeout()), this, SLOT(on_Maininterface_hide()));
    }
    int num = 0;
    if ( this->vboxlayout != NULL )
    {
        QLayoutItem* item;
        while ((item = this->vboxlayout->takeAt(0)) != NULL)
        {
            delete item->widget();
            delete item;
        }
    }
    qDebug()<<"reason"<<reason;
    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::Context:
      if (this->isHidden())
      {
          //MainWindow::hign = MainWindow::oneVolumeDriveNum*98+MainWindow::twoVolumeDriveNum*110+MainWindow::threeVolumeDriveNum*130+MainWindow::fourVolumeDriveNum*160;
          for(auto cacheDrive : *findGDriveList())
          {
              qDebug()<<"findGDriveList():"<<findGDriveList()->size();
              qDebug()<<"findGVolumeList():"<<findGVolumeList()->size();
              hign = findGVolumeList()->size()*40 + findGDriveList()->size()*55;
              this->setFixedSize(280,hign);
              int DisNum = g_list_length(g_drive_get_volumes(cacheDrive));
              if (DisNum >0 )
              {
                if (g_drive_can_eject(cacheDrive) || g_drive_can_stop(cacheDrive))
                {

                    if(DisNum == 1)
                    {
                       //this->resize(250, 98);
                       num++;
                       UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                       QByteArray date = UDiskPathDis1.toLocal8Bit();
                       char *p_Change = date.data();
                       GFile *file = g_file_new_for_path(p_Change);
                       GFileInfo *info = g_file_query_filesystem_info(file,"*",NULL,NULL);
                       totalDis1 = g_file_info_get_attribute_uint64(info,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
                       if(findGDriveList()->size() == 1)
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL, totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL,1);
                       }
                       else if(num == 1)
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL, totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL,1);
                       }
                       else
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL,totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL, 2);
                       }
                    }
                    if(DisNum == 2)
                    {
                        num++;
                        //this->resize(250,160);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,2);
                        }

                    }
                    if(DisNum == 3)
                    {
                        num++;
                        //this->resize(250,222);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis3 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2))));
                        QByteArray dateDis3 = UDiskPathDis3.toLocal8Bit();
                        char *p_ChangeDis3 = dateDis3.data();
                        GFile *fileDis3 = g_file_new_for_path(p_ChangeDis3);
                        GFileInfo *infoDis3 = g_file_query_filesystem_info(fileDis3,"*",NULL,NULL);
                        totalDis3 = g_file_info_get_attribute_uint64(infoDis3,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,2);
                        }

                    }
                    if(DisNum == 4)
                    {
                        num++;
                        //this->resize(250,134);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis3 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2))));
                        QByteArray dateDis3 = UDiskPathDis3.toLocal8Bit();
                        char *p_ChangeDis3 = dateDis3.data();
                        GFile *fileDis3 = g_file_new_for_path(p_ChangeDis3);
                        GFileInfo *infoDis3 = g_file_query_filesystem_info(fileDis3,"*",NULL,NULL);
                        totalDis3 = g_file_info_get_attribute_uint64(infoDis3,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis4 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3))));
                        QByteArray dateDis4 = UDiskPathDis4.toLocal8Bit();
                        char *p_ChangeDis4 = dateDis4.data();
                        GFile *fileDis4 = g_file_new_for_path(p_ChangeDis4);
                        GFileInfo *infoDis4 = g_file_query_filesystem_info(fileDis4,"*",NULL,NULL);
                        totalDis4 = g_file_info_get_attribute_uint64(infoDis4,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,2);
                        }

                    }
                    connect(open_widget, &QClickWidget::clickedConvert,[=]()
                    {
                        qDebug()<<"相应信号";
                        g_drive_eject_with_operation(cacheDrive,
                                     G_MOUNT_UNMOUNT_NONE,
                                     NULL,
                                     NULL,
                                     GAsyncReadyCallback(frobnitz_result_func),
                                     this);
                        this->hide();
                        ejectInterface *ForEject = new ejectInterface(NULL,g_drive_get_name(cacheDrive));
                        ForEject->show();
                        QLayoutItem* item;
                        while ((item = this->vboxlayout->takeAt(0)) != NULL)
                        {
                            delete item->widget();
                            delete item;
                        }
                    });
                }

                if(findGDriveList()->size() != 0 || findGDriveList()->size() != 0)
                {
                    this->showNormal();
                    moveBottomRight();
                }
            }


      }
    }
    else
    {
      qDebug()<<"hidden now" ;
      this->hide();
    }
    break;
    default:
        break;
    }
    qDebug()<<"1111111111111111111 now" ;
    ui->centralWidget->show();
}

void MainWindow::newarea(int No,
                         QString Drivename,
                         QString nameDis1,
                         QString nameDis2,
                         QString nameDis3,
                         QString nameDis4,
                         qlonglong capacityDis1,
                         qlonglong capacityDis2,
                         qlonglong capacityDis3,
                         qlonglong capacityDis4,
                         QString pathDis1,
                         QString pathDis2,
                         QString pathDis3,
                         QString pathDis4,
                         int linestatus)
{
    open_widget = new QClickWidget(NULL,No,Drivename,nameDis1,nameDis2,nameDis3,nameDis4,
                                   capacityDis1,capacityDis2,capacityDis3,capacityDis4,
                                   pathDis1,pathDis2,pathDis3,pathDis4);
    //open_widget->;

    QWidget *line = new QWidget;
    line->setFixedHeight(1);
    line->setObjectName("lineWidget");
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (linestatus == 2)
    {
        this->vboxlayout->addWidget(line);
    }

    this->vboxlayout->addWidget(open_widget);
    vboxlayout->setContentsMargins(2,8,2,8);

    if (linestatus == 0)
    {
        this->vboxlayout->addWidget(line);
    }

    open_widget->setStyleSheet(
                //正常状态样式
                "QClickWidget{"
                "height:79px;"
                "}"
                "QClickWidget:hover{"
                "background-color:rgba(255,255,255,30);"
                "}"
                );

}

void MainWindow::moveBottomRight()
{
////////////////////////////////////////get the loacation of the mouse
    /*QPoint globalCursorPos = QCursor::pos();

    QRect availableGeometry = qApp->primaryScreen()->availableGeometry();
    QRect screenGeometry = qApp->primaryScreen()->geometry();

    QDesktopWidget* pDesktopWidget = QApplication::desktop();

    QRect screenRect = pDesktopWidget->screenGeometry();//area of the screen

    if (screenRect.height() != availableGeometry.height())
    {
        this->move(globalCursorPos.x()-125, availableGeometry.height() - this->height());
    }
    else
    {
        this->move(globalCursorPos.x()-125, availableGeometry.height() - this->height() - 40);
    }*/
    //////////////////////////////////////origin code base on the location of the mouse
//    int screenNum = QGuiApplication::screens().count();
//    qDebug()<<"ScreenNum:"<<"-------------"<<screenNum;
//    int panelHeight = getPanelHeight("PanelHeight");
//    int position =0;
//    position = getPanelPosition("PanelPosion");
//    int screen = 0;
//    QRect rect;
//    int localX ,availableWidth,totalWidth;
//    int localY,availableHeight,totalHeight;

//    qDebug() << "任务栏位置"<< position;
//    if (screenNum > 1)
//    {
//        if (position == rightPosition)                                  //on the right
//        {
//            screen = screenNum - 1;

//            //Available screen width and height
//            availableWidth =QGuiApplication::screens().at(screen)->geometry().x() +  QGuiApplication::screens().at(screen)->size().width()-panelHeight;
//            qDebug()<<QGuiApplication::screens().at(screen)->geometry().x()<<"QGuiApplication::screens().at(screen)->geometry().x()";
//            qDebug()<<QGuiApplication::screens().at(screen)->size().width()<<"QGuiApplication::screens().at(screen)->size().width()";
//            availableHeight = QGuiApplication::screens().at(screen)->availableGeometry().height();

//            //total width
//            totalWidth =  QGuiApplication::screens().at(0)->size().width() + QGuiApplication::screens().at(screen)->size().width();
//            totalHeight = QGuiApplication::screens().at(screen)->size().height();
//            this->move(totalWidth,totalHeight);
//        }
//        else if(position  ==downPosition || upPosition ==1)                  //above or bellow
//        {
//            availableHeight = QGuiApplication::screens().at(0)->size().height() - panelHeight;
//            availableWidth = QGuiApplication::screens().at(0)->size().width();
//            totalHeight = QGuiApplication::screens().at(0)->size().height();
//            totalWidth = QGuiApplication::screens().at(0)->size().width();
//        }
//        else
//        {
//            availableHeight = QGuiApplication::screens().at(0)->availableGeometry().height();
//            availableWidth = QGuiApplication::screens().at(0)->availableGeometry().width();
//            totalHeight = QGuiApplication::screens().at(0)->size().height();
//            totalWidth = QGuiApplication::screens().at(0)->size().width();
//        }
//    }

//    else
//    {
//        availableHeight = QGuiApplication::screens().at(0)->availableGeometry().height();
//        availableWidth = QGuiApplication::screens().at(0)->availableGeometry().width();
//        totalHeight = QGuiApplication::screens().at(0)->size().height();
//        totalWidth = QGuiApplication::screens().at(0)->size().width();

//        //show the location of the systemtray
//        rect = m_systray->geometry();
//        localX = rect.x() - (this->width()/2 - rect.size().width()/2) ;
//        localY = availableHeight - this->height();

//        //modify location
//        if (position == downPosition)
//        { //下
//            if (availableWidth - rect.x() - rect.width()/2 < this->width() / 2)
//                this->setGeometry(availableWidth-this->width(),availableHeight-this->height()-DistanceToPanel,this->width(),this->height());
//            else
//                this->setGeometry(localX,availableHeight-this->height()-DistanceToPanel,this->width(),this->height());
//        }
//        else if (position == upPosition)
//        { //上
//            if (availableWidth - rect.x() - rect.width()/2 < this->width() / 2)
//                this->setGeometry(availableWidth-this->width(),totalHeight-availableHeight+DistanceToPanel,this->width(),this->height());
//            else
//                this->setGeometry(localX,totalHeight-availableHeight+DistanceToPanel,this->width(),this->height());
//        }
//        else if (position == leftPosition)
//        {
//            if (availableHeight - rect.y() - rect.height()/2 > this->height() /2)
//                this->setGeometry(panelHeight + DistanceToPanel,rect.y() + (rect.width() /2) -(this->height()/2) ,this->width(),this->height());
//            else
//                this->setGeometry(panelHeight+DistanceToPanel,localY,this->width(),this->height());//左
//        }
//        else if (position == rightPosition)
//        {
//            localX = availableWidth - this->width();
//            if (availableHeight - rect.y() - rect.height()/2 > this->height() /2)
//                this->setGeometry(availableWidth - this->width() -DistanceToPanel,rect.y() + (rect.width() /2) -(this->height()/2),this->width(),this->height());
//            else
//                this->setGeometry(localX-DistanceToPanel,localY,this->width(),this->height());
//        }
//    }
    QRect availableGeometry = qApp->primaryScreen()->availableGeometry();
    QRect screenGeometry = qApp->primaryScreen()->geometry();

    QDesktopWidget* desktopWidget = QApplication::desktop();
    QRect deskMainRect = desktopWidget->availableGeometry(0);//获取可用桌面大小
    QRect screenMainRect = desktopWidget->screenGeometry(0);//获取设备屏幕大小
    QRect deskDupRect = desktopWidget->availableGeometry(1);//获取可用桌面大小
    QRect screenDupRect = desktopWidget->screenGeometry(1);//获取设备屏幕大小

//    qDebug()<<"                                                  ";
//    qDebug()<<"trayIcon:"<<trayIcon->geometry();
//    qDebug()<<"screenGeometry: "<<screenGeometry;
//    qDebug()<<"availableGeometry: "<<availableGeometry;

//    qDebug()<<"deskMainRect: "<<deskMainRect;
//    qDebug()<<"screenMainRect: "<<screenMainRect;
//    qDebug()<<"deskDupRect: "<<deskDupRect;
//    qDebug()<<"screenDupRect: "<<screenDupRect;

    int panelHeight = getPanelHeight("PanelHeight");
    int position =0;
    position = getPanelPosition("PanelPosion");

    if (screenGeometry.width() == availableGeometry.width() && screenGeometry.height() == availableGeometry.height())
    {
        qDebug()<<"availableGeometry.width():"<<availableGeometry.width();
        qDebug()<<"screenGeometry.height():"<<screenGeometry.height();
        if(position == downPosition)
        {
            //任务栏在下侧
            this->move(availableGeometry.x() + availableGeometry.width() - this->width(), screenMainRect.y() + availableGeometry.height() - this->height() - panelHeight - DistanceToPanel);
        }
        else if(position == upPosition)
        {
            //任务栏在上侧
            this->move(availableGeometry.x() + availableGeometry.width() - this->width(), screenMainRect.y() + screenGeometry.height() - availableGeometry.height() + panelHeight + DistanceToPanel);
        }
        else if (position == leftPosition)
        {
            //任务栏在左侧
            this->move(panelHeight + DistanceToPanel, screenMainRect.y() + screenMainRect.height() - this->height());
//            if (screenGeometry.x() == 0){//主屏在左侧
//                this->move(screenGeometry.width() - availableGeometry.width() + m + d, screenMainRect.y() + screenMainRect.height() - this->height());
//            }else{//主屏在右侧
//                this->move(screenGeometry.width() - availableGeometry.width() + m + d,screenDupRect.y() + screenDupRect.height() - this->height());
//            }
        }
        else if (position == rightPosition)
        {
            //任务栏在右侧
            this->move(screenMainRect.width() - this->width() - panelHeight - DistanceToPanel, screenMainRect.y() + screenMainRect.height() - this->height());
//            if (screenGeometry.x() == 0){//主屏在左侧
//                this->move(screenMainRect.width() + screenDupRect.width() - this->width() - m - d, screenDupRect.y() + screenDupRect.height() - this->height());
//            }else{//主屏在右侧
//                this->move(availableGeometry.x() + availableGeometry.width() - this->width() - m - d, screenMainRect.y() + screenMainRect.height() - this->height());
//            }
        }
    }
    else if(screenGeometry.width() == availableGeometry.width() )
    {
        qDebug()<<"if i have come in"<<screenGeometry.width();
        qDebug()<<"if i have come in"<<availableGeometry.width();
        if (m_systray->geometry().y() > availableGeometry.height()/2)
        {
            //任务栏在下侧
            this->move(availableGeometry.x() + availableGeometry.width() - this->width(), screenMainRect.y() + availableGeometry.height() - this->height() - DistanceToPanel);
        }else{
            //任务栏在上侧
            this->move(availableGeometry.x() + availableGeometry.width() - this->width(), screenMainRect.y() + screenGeometry.height() - availableGeometry.height() + DistanceToPanel);
        }
    }
    else if (screenGeometry.height() == availableGeometry.height())
    {
        if (m_systray->geometry().x() > availableGeometry.width()/2)
        {
            //任务栏在右侧
            this->move(availableGeometry.x() + availableGeometry.width() - this->width() - DistanceToPanel, screenMainRect.y() + screenGeometry.height() - this->height());
        }
        else
        {
            //任务栏在左侧
            this->move(screenGeometry.width() - availableGeometry.width() + DistanceToPanel, screenMainRect.y() + screenGeometry.height() - this->height());
        }
    }

}

/*
    use dbus to get the location of the panel
*/
int MainWindow::getPanelPosition(QString str)
{
    QDBusInterface interface( "com.ukui.panel.desktop",
                              "/",
                              "com.ukui.panel.desktop",
                              QDBusConnection::sessionBus() );
    QDBusReply<int> reply = interface.call("GetPanelPosition", str);

    return reply;
}

/*
    use the dbus to get the height of the panel
*/
int MainWindow::getPanelHeight(QString str)
{
    QDBusInterface interface( "com.ukui.panel.desktop",
                              "/",
                              "com.ukui.panel.desktop",
                              QDBusConnection::sessionBus() );
    QDBusReply<int> reply = interface.call("GetPanelSize", str);
    return reply;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    qDebug()<<screen->availableGeometry()<<"123456789";
    qDebug()<<screen->availableSize()<<"987654321";
    qDebug()<<screen->size().width()<<"-------------";
    qDebug()<<screen->size().height()<<"+++++++++++++";
}

void MainWindow::moveBottomDirect(GDrive *drive)
{
//    ejectInterface *ForEject = new ejectInterface(NULL,g_drive_get_name(drive));
//    int screenNum = QGuiApplication::screens().count();
//    int panelHeight = getPanelHeight("PanelHeight");
//    int position =0;
//    position = getPanelPosition("PanelPosion");
//    int screen = 0;
//    QRect rect;
//    int localX ,availableWidth,totalWidth;
//    int localY,availableHeight,totalHeight;

//    qDebug() << "任务栏位置"<< position;
//    if (screenNum > 1)
//    {
//        if (position == rightPosition)                                  //on the right
//        {
//            screen = screenNum - 1;

//            //Available screen width and height
//            availableWidth =QGuiApplication::screens().at(screen)->geometry().x() +  QGuiApplication::screens().at(screen)->size().width()-panelHeight;
//            availableHeight = QGuiApplication::screens().at(screen)->availableGeometry().height();

//            //total width
//            totalWidth =  QGuiApplication::screens().at(0)->size().width() + QGuiApplication::screens().at(screen)->size().width();
//            totalHeight = QGuiApplication::screens().at(screen)->size().height();
//        }
//        else if(position  ==downPosition || position ==upPosition)                  //above or bellow
//        {
//            availableHeight = QGuiApplication::screens().at(0)->size().height() - panelHeight;
//            availableWidth = QGuiApplication::screens().at(0)->size().width();
//            totalHeight = QGuiApplication::screens().at(0)->size().height();
//            totalWidth = QGuiApplication::screens().at(0)->size().width();
//        }
//        else
//        {
//            availableHeight = QGuiApplication::screens().at(0)->availableGeometry().height();
//            availableWidth = QGuiApplication::screens().at(0)->availableGeometry().width();
//            totalHeight = QGuiApplication::screens().at(0)->size().height();
//            totalWidth = QGuiApplication::screens().at(0)->size().width();
//        }
//    }

//    else
//    {
//        availableHeight = QGuiApplication::screens().at(0)->availableGeometry().height();
//        availableWidth = QGuiApplication::screens().at(0)->availableGeometry().width();
//        totalHeight = QGuiApplication::screens().at(0)->size().height();
//        totalWidth = QGuiApplication::screens().at(0)->size().width();
//    }
//    //show the location of the systemtray
//    rect = m_systray->geometry();
//    localX = rect.x() - (ForEject->width()/2 - rect.size().width()/2) ;
//    localY = availableHeight - ForEject->height();
//    //modify location
//    if (position == downPosition)
//    { //下
//        if (availableWidth - rect.x() - rect.width()/2 < ForEject->width() / 2)
//            ForEject->setGeometry(availableWidth-ForEject->width(),availableHeight-ForEject->height()-DistanceToPanel,ForEject->width(),ForEject->height());
//        else
//            ForEject->setGeometry(localX-16,availableHeight-ForEject->height()-DistanceToPanel,ForEject->width(),ForEject->height());
//    }
//    else if (position == upPosition)
//    { //上
//        if (availableWidth - rect.x() - rect.width()/2 < ForEject->width() / 2)
//            ForEject->setGeometry(availableWidth-ForEject->width(),totalHeight-availableHeight+DistanceToPanel,ForEject->width(),ForEject->height());
//        else
//            ForEject->setGeometry(localX-16,totalHeight-availableHeight+DistanceToPanel,ForEject->width(),ForEject->height());
//    }
//    else if (position == leftPosition)
//    {
//        if (availableHeight - rect.y() - rect.height()/2 > ForEject->height() /2)
//            ForEject->setGeometry(panelHeight + DistanceToPanel,rect.y() + (rect.width() /2) -(ForEject->height()/2) ,ForEject->width(),ForEject->height());
//        else
//            ForEject->setGeometry(panelHeight+DistanceToPanel,localY,ForEject->width(),ForEject->height());//左
//    }
//    else if (position == rightPosition)
//    {
//        localX = availableWidth - ForEject->width();
//        if (availableHeight - rect.y() - rect.height()/2 > ForEject->height() /2)
//        {
//            ForEject->setGeometry(availableWidth - ForEject->width() -DistanceToPanel,rect.y() + (rect.height() /2) -(ForEject->height()/2),ForEject->width(),ForEject->height());
//        }
//        else
//            ForEject->setGeometry(localX-DistanceToPanel,localY,ForEject->width(),ForEject->height());
//    }
//    ForEject->show();
}

void MainWindow::MainWindowShow()
{
    int num = 0;
    if  ( this->vboxlayout != NULL )
    {
        QLayoutItem* item;
        while ((item = this->vboxlayout->takeAt(0)) != NULL)
        {
            delete item->widget();
            delete item;
        }
    }
//      if (this->isHidden())
//      {

          //MainWindow::hign = MainWindow::oneVolumeDriveNum*98+MainWindow::twoVolumeDriveNum*110+MainWindow::threeVolumeDriveNum*130+MainWindow::fourVolumeDriveNum*160;
          for(auto cacheDrive : *findGDriveList())
          {

              qDebug()<<"findGVolumeList()->size():"<<findGVolumeList()->size();
              qDebug()<<"findGDriveList()->size():"<<findGDriveList()->size();
              hign = findGVolumeList()->size()*40 + findGDriveList()->size()*55;
              qDebug()<<hign<<"the mainwindow's high";
              this->setFixedSize(280,hign);
              g_drive_get_volumes(cacheDrive);
              int DisNum = g_list_length(g_drive_get_volumes(cacheDrive));
              if (DisNum >0 )
              {
                if (g_drive_can_eject(cacheDrive) || g_drive_can_stop(cacheDrive))
                {

                    if(DisNum == 1)
                    {
                       //this->resize(250, 98);
                       num++;
                       UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                       QByteArray date = UDiskPathDis1.toLocal8Bit();
                       char *p_Change = date.data();
                       GFile *file = g_file_new_for_path(p_Change);
                       GFileInfo *info = g_file_query_filesystem_info(file,"*",NULL,NULL);
                       totalDis1 = g_file_info_get_attribute_uint64(info,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
                       if(findGDriveList()->size() == 1)
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL, totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL,1);
                       }
                       else if(num == 1)
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL, totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL,1);
                       }
                       else
                       {
                           newarea(DisNum,g_drive_get_name(cacheDrive),
                                   g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                   NULL,NULL,NULL,totalDis1,NULL,NULL,NULL, UDiskPathDis1,NULL,NULL,NULL, 2);
                       }
                    }
                    if(DisNum == 2)
                    {
                        num++;
                        //this->resize(250,160);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    NULL,NULL, totalDis1,totalDis2,NULL,NULL, UDiskPathDis1,UDiskPathDis2,NULL,NULL,2);
                        }

                    }
                    if(DisNum == 3)
                    {
                        num++;
                        //this->resize(250,222);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis3 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2))));
                        QByteArray dateDis3 = UDiskPathDis3.toLocal8Bit();
                        char *p_ChangeDis3 = dateDis3.data();
                        GFile *fileDis3 = g_file_new_for_path(p_ChangeDis3);
                        GFileInfo *infoDis3 = g_file_query_filesystem_info(fileDis3,"*",NULL,NULL);
                        totalDis3 = g_file_info_get_attribute_uint64(infoDis3,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    NULL, totalDis1,totalDis2,totalDis3,NULL, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,NULL,2);
                        }

                    }
                    if(DisNum == 4)
                    {
                        num++;
                        //this->resize(250,134);
                        UDiskPathDis1 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0))));
                        QByteArray dateDis1 = UDiskPathDis1.toLocal8Bit();
                        char *p_ChangeDis1 = dateDis1.data();
                        GFile *fileDis1 = g_file_new_for_path(p_ChangeDis1);
                        GFileInfo *infoDis1 = g_file_query_filesystem_info(fileDis1,"*",NULL,NULL);
                        totalDis1 = g_file_info_get_attribute_uint64(infoDis1,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis2 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1))));
                        QByteArray dateDis2 = UDiskPathDis2.toLocal8Bit();
                        char *p_ChangeDis2 = dateDis2.data();
                        GFile *fileDis2 = g_file_new_for_path(p_ChangeDis2);
                        GFileInfo *infoDis2 = g_file_query_filesystem_info(fileDis2,"*",NULL,NULL);
                        totalDis2 = g_file_info_get_attribute_uint64(infoDis2,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis3 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2))));
                        QByteArray dateDis3 = UDiskPathDis3.toLocal8Bit();
                        char *p_ChangeDis3 = dateDis3.data();
                        GFile *fileDis3 = g_file_new_for_path(p_ChangeDis3);
                        GFileInfo *infoDis3 = g_file_query_filesystem_info(fileDis3,"*",NULL,NULL);
                        totalDis3 = g_file_info_get_attribute_uint64(infoDis3,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        UDiskPathDis4 = g_file_get_path(g_mount_get_root(g_volume_get_mount((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3))));
                        QByteArray dateDis4 = UDiskPathDis4.toLocal8Bit();
                        char *p_ChangeDis4 = dateDis4.data();
                        GFile *fileDis4 = g_file_new_for_path(p_ChangeDis4);
                        GFileInfo *infoDis4 = g_file_query_filesystem_info(fileDis4,"*",NULL,NULL);
                        totalDis4 = g_file_info_get_attribute_uint64(infoDis4,G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

                        if(findGDriveList()->size() == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,1);
                        }

                        else if(num == 1)
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,1);
                        }

                        else
                        {
                            newarea(DisNum,g_drive_get_name(cacheDrive),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),0)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),1)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),2)),
                                    g_volume_get_name((GVolume *)g_list_nth_data(g_drive_get_volumes(cacheDrive),3)),
                                    totalDis1,totalDis2,totalDis3,totalDis4, UDiskPathDis1,UDiskPathDis2,UDiskPathDis3,UDiskPathDis4,2);
                        }

                    }
                    connect(open_widget, &QClickWidget::clickedConvert,[=]()
                    {
                        qDebug()<<"相应信号";

                        g_drive_eject_with_operation(cacheDrive,
                                     G_MOUNT_UNMOUNT_NONE,
                                     NULL,
                                     NULL,
                                     GAsyncReadyCallback(frobnitz_result_func),
                                     this);

                        findGDriveList()->removeOne(cacheDrive);
                        qDebug()<<"findGDriveList()->size():"<<findGDriveList()->size();
                        this->hide();
                        QLayoutItem* item;
                        while ((item = this->vboxlayout->takeAt(0)) != NULL)
                        {
                            delete item->widget();
                            delete item;
                        }

                        //hign = findList()->size()*50+30;

                    });
                }

                if(findGDriveList()->size() != 0 || findGDriveList() != 0)
                {
                    this->showNormal();
                    moveBottomNoBase();
                }
            }


      }
//    }
//        else
//        {
//            this->hide();
//        }
    ui->centralWidget->show();
    interfaceHideTime->setTimerType(Qt::PreciseTimer);
    if(ui->centralWidget != NULL)
    {
        connect(interfaceHideTime, SIGNAL(timeout()), this, SLOT(on_Maininterface_hide()));
    }
    interfaceHideTime->start(2000);

}

void MainWindow::on_Maininterface_hide()
{
    qDebug()<<"hiddddddddddddddddddd";
    this->hide();
    interfaceHideTime->stop();
}

void MainWindow::moveBottomNoBase()
{
    screen->availableGeometry();
    screen->availableSize();
    if(screen->availableGeometry().x() == screen->availableGeometry().y() && screen->availableSize().height() < screen->size().height())
    {
        qDebug()<<"the positon of panel is down";
        this->move(screen->availableGeometry().x() + screen->size().width() -
                   this->width() - DistanceToPanel,screen->availableGeometry().y() +
                   screen->availableSize().height() - this->height() - DistanceToPanel);
    }

    if(screen->availableGeometry().x() < screen->availableGeometry().y() && screen->availableSize().height() < screen->size().height())
    {
        qDebug()<<"this position of panel is up";
        this->move(screen->availableGeometry().x() + screen->size().width() -
                   this->width() - DistanceToPanel,screen->availableGeometry().y());
    }

    if(screen->availableGeometry().x() > screen->availableGeometry().y() && screen->availableSize().width() < screen->size().width())
    {
        qDebug()<<"this position of panel is left";
        this->move(screen->availableGeometry().x() + DistanceToPanel,screen->availableGeometry().y()
                   + screen->availableSize().height() - this->height());
    }

    if(screen->availableGeometry().x() == screen->availableGeometry().y() && screen->availableSize().width() < screen->size().width())
    {
        qDebug()<<"this position of panel is right";
        this->move(screen->availableGeometry().x() + screen->availableSize().width() -
                   DistanceToPanel - this->width(),screen->availableGeometry().y() +
                   screen->availableSize().height() - (this->height())*(DistanceToPanel - 1));
    }
}