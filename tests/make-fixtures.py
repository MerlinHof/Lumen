import os
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import html
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
base=Path('work/fixtures');base.mkdir(exist_ok=True)
chapters=[('Ein guter Anfang','Äpfel, Öl und überraschende Bücher. Ä Ö Ü ä ö ü ß. Auch zusammengesetzte Zeichen: A\u0308 O\u0308 U\u0308 a\u0308 o\u0308 u\u0308. „Ruhig lesen“ – ohne Ablenkung.'),('Eine Seite weiter','Dieses Kapitel prüft Navigation, Schriftgröße und Leseposition.'),('Zeit zum Lesen','LUMEN ist eine eigene Oberfläche und verwendet MuPDF ausschließlich als Renderingbibliothek.')]
with ZipFile(base/'LUMEN Lesetest.epub','w') as z:
 z.writestr('mimetype','application/epub+zip')
 z.writestr('META-INF/container.xml','<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
 items=''.join(f'<item id="ch{i}" href="ch{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(3))
 spine=''.join(f'<itemref idref="ch{i}"/>' for i in range(3))
 z.writestr('OEBPS/content.opf',f'''<?xml version="1.0" encoding="UTF-8"?><package version="3.0" xmlns="http://www.idpf.org/2007/opf" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">lumen-test-v2</dc:identifier><dc:title>Äpfel, Öl und Bücher</dc:title><dc:creator>LUMEN</dc:creator><dc:language>de</dc:language><meta property="dcterms:modified">2026-10-26T12:00:00Z</meta></metadata><manifest>{items}<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/></manifest><spine>{spine}</spine></package>''')
 nav=''.join(f'<li><a href="ch{i}.xhtml">{title}</a></li>' for i,(title,_) in enumerate(chapters))
 z.writestr('OEBPS/nav.xhtml',f'<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Inhalt</title></head><body><nav epub:type="toc"><ol>{nav}</ol></nav></body></html>')
 for i,(title,text) in enumerate(chapters):
  paras=''.join(f'<p>{html.escape(text)} Absatz {n+1}.</p>' for n in range(12))
  z.writestr(f'OEBPS/ch{i}.xhtml',f'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title></head><body><h1>{title}</h1>{paras}</body></html>')
font=os.environ.get('LUMEN_TEST_FONT','/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf')
pdfmetrics.registerFont(TTFont('Serif',font))
c=canvas.Canvas(str(base/'layout.pdf'));c.setTitle('PDF-Lesetest mit Umlauten');c.setAuthor('LUMEN')
for i in range(3):
 c.setFont('Serif',24);c.drawString(48,760,f'Seite {i+1}: Ä Ö Ü ä ö ü ß')
 c.setFont('Serif',14)
 for j in range(18):c.drawString(48,710-j*30,'Ein PDF mit Text, Seitenwechsel, Zoom und Verschieben.')
 c.bookmarkPage(str(i));c.addOutlineEntry(f'Kapitel {i+1}',str(i));c.showPage()
c.save()
(base/'utf8.txt').write_text(('Äpfel, Öl und Bücher – ä ö ü ß.\nA\u0308 O\u0308 U\u0308 a\u0308 o\u0308 u\u0308.\n\n'*100),encoding='utf8')
(base/'broken.epub').write_bytes(b'Not a ZIP file')
