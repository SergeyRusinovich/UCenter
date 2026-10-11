// Растрирует SVG-иконку: render_icons <svg> <outdir> -> ucenter.iconset (PNG для iconutil),
// ucenter.png (256 px, значок окна) и ucenter.ico (Windows). Запускается из render.sh.
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QSvgRenderer>
static QImage render(QSvgRenderer &r, int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    return img;
}
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QSvgRenderer r(QString::fromLocal8Bit(argv[1]));
    QDir out(QString::fromLocal8Bit(argv[2]));
    out.mkpath("ucenter.iconset");
    for (int s : {16, 32, 128, 256, 512}) {
        render(r, s).save(out.filePath(QString("ucenter.iconset/icon_%1x%1.png").arg(s)));
        render(r, s * 2).save(out.filePath(QString("ucenter.iconset/icon_%1x%1@2x.png").arg(s)));
    }
    render(r, 256).save(out.filePath("ucenter.png"));
    QImageWriter ico(out.filePath("ucenter.ico"), "ico");
    if (!ico.write(render(r, 256)))
        qWarning() << ico.errorString();
    return 0;
}
