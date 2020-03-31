
#include <math.h>
#include "stringutil.h"
#include "firengineglobals.h"


static const char* s_pCSSText[] = {
	"<style type=\"text/css\">",
	"h1 {",
	"	font-size: 36pt;",
	"	color:#1C1259;",
	"	line-height: 1em;",
	"	margin-bottom: 1em;",
	"}",
	"h2 {",
	"	font-size: 24pt;",
	"	color:#1C1259;",
	"	line-height: 3em;",
	"	margin-bottom: 2em;",
	"}",
	"h2:after {",
	"	content:' ';",
	"    display:block;",
	"    border:2px solid black;",
	"	line-height: 3em;",
	"}",
	"h3 {",
	"	font-size: 16pt;",
	"	color:#1C1259;",
	"}",
	"h3:after {",
	"	content:' ';",
	"    display:block;",
	"    border:1px solid black;",
	"}",
	"body {",
	"	color: #666;",
	"	font: 12pt \"Open Sans\", \"HelveticaNeue - Light\", \"Helvetica Neue Light\", \"Helvetica Neue\", Helvetica, Arial, \"Lucida Grande\", Sans-Serif;",
	"	margin-right: 10pt;",
	"	margin-left: 10pt;",
	"	margin-top: 6.0pt;",
	"	margin-bottom: 6.0pt;",
	"}",
	"input {",
	"	color: #666;",
	"	font: 12pt \"Open Sans\", \"HelveticaNeue - Light\", \"Helvetica Neue Light\", \"Helvetica Neue\", Helvetica, Arial, \"Lucida Grande\", Sans-Serif;",
	"	margin-right: 10pt;",
	"	margin-left: 10pt;",
	"	margin-top: 6.0pt;",
	"	margin-bottom: 6.0pt;",
	"}",
	"p {",
	"	margin-left: 90.0pt;",
	"}",
	"table {",
	"	margin-top: 12.0pt;",
	"	margin-bottom: 12.0pt;",
	"	margin-left: 90.0pt;",
	"	border-collapse: separate;",
	"	border-spacing: 0;",
	"}",
	"table.t1 td, th {",
	"  padding: 6px 15px;",
	"  text-align: center;",
	"}",
	"table.t1 th {",
	"  background: #1C1259;",
	"  color: #fff;",
	"  text-align: center;",
	"}",
	"table.t1 tr:first-child th:first-child {",
	"  border-top-left-radius: 6px;",
	"}",
	"table.t1 td:first-child {",
	"  font: 16px Courier;",
	"}",
	"table.tcode td {",
	"  font: 16px Courier;",
	"  text-align: left;",
	"  color: black;",
	"}",
	"table.t1 tr:first-child th:last-child {",
	"  border-top-right-radius: 6px;",
	"}",
	"table.t1 td {",
	"  border-right: 1px solid #c6c9cc;",
	"  border-bottom: 1px solid #c6c9cc;",
	"}",
	"table.t1 td:first-child {",
	"  border-left: 1px solid #c6c9cc;",
	"}",
	"table.t1 tr:first-child td {",
	"  border-top: 1px solid #c6c9cc;",
	"}",
	"table.t1 tr:nth-child(even) td {",
	"  background: #eaeaed;",
	"}",
	"table.t1 tr:last-child td:first-child {",
	"  border-bottom-left-radius: 6px;",
	"}",
	"table.t1 tr:last-child td:last-child {",
	"  border-bottom-right-radius: 6px;",
	"}",
	"</style>",
	0 };

void FirEngineGlobals::renderHtmlHeader(ostream& stream)
{
	stream << "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n";
	stream << "<html>\n";
	stream << "<head>\n";
	stream << "<title>FirEngine Design</title>\n";
	stream << endl;

	// Output Embedded CSS Style
	for (unsigned i = 0; s_pCSSText[i]; ++i)
		stream << s_pCSSText[i] << "\n";
	stream << endl;

	stream << "<script type=\"text/javascript\" src=\"../../canvasjs/canvasjs.min.js\"></script>\n";

	stream << "</head>" << "\n\n";
	stream << "<body>" << "\n";
	stream << "\n";
}

void FirEngineGlobals::renderHtmlFooter(ostream& stream)
{
	stream << "\n";
	stream << "</body>" << "\n\n";
	stream << "</html>" << "\n";
}

string FirEngineGlobals::getHtmlRainbowColor(double f)
{
	double X = floor(f * 5.0);
	double Y = ((f * 5.0) - X) * 255.0;
	char cx = char(X);
	char cy = char(Y);

	unsigned char r, g, b;
	switch (cx)
	{
	case 0: r = 255; g = cy; b = 0; break;
	case 1: r = 255 - cy; g = 255; b = 0; break;
	case 2: r = 0; g = 255; b = cy; break;
	case 3: r = 0; g = 255 - cy; b = 255; break;
	case 4: r = cy; g = 0; b = 255; break;
	case 5: r = 255; g = 0; b = 255; break;
	}

	unsigned rgb = (r << 16) + (g << 8) + b;
	return string("#") + toHexDigits(rgb, 6);
}
