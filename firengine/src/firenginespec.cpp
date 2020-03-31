
#include <assert.h>
#include "stringutil.h"
#include "stringmatchstream.h"
#include "firenginespec.h"
#include "fircoeffref.h"


FirEngineSpec::FirEngineSpec(double clockFreq) :
	m_ClockFreq			(clockFreq),
	m_vFirSpec			()
{
}
	
double FirEngineSpec::lookupCoeff(const FirCoeffRef& firCoeffRef) const
{
	assert(!firCoeffRef.isNull());
	const FirSpec& firSpec = m_vFirSpec[firCoeffRef.m_FirIndex];
	double coeffVal = firSpec.m_vCoeff[firCoeffRef.m_CoeffIndex];
	return coeffVal;
}

void FirEngineSpec::readFromFile(istream& stream)
{
	unsigned lineNum = 1;
	
	try
	{
		string lineStr;
		for (; safeGetline(stream, lineStr); ++lineNum) if (!lineStr.empty())
		{
			StringMatchStream matchStream(lineStr);
			matchStream.matchWhitespace();

			if (matchStream.atEnd())
				;
			else if (*matchStream == '#')		// comment 
				;
			else if (matchStream.matchText("FIR"))
			{
				unsigned firIdx = 0;
				matchStream.matchWhitespace();
				if (!matchStream.matchChar('['))
					throw string("Syntax Error: Expected '['");
				matchStream.matchWhitespace();
				if (!matchStream.matchUInt(&firIdx))
					throw string("Syntax Error: Expected FIR-Index");
				matchStream.matchWhitespace();
				if (!matchStream.matchChar(']'))
					throw string("Syntax Error: Expected ']'");
				matchStream.matchWhitespace();
				if (!matchStream.matchChar('.'))
					throw string("Syntax Error: Expected '.'");

				while (m_vFirSpec.size() <= firIdx)
					m_vFirSpec.push_back(FirSpec());
				FirSpec& firSpec = m_vFirSpec[firIdx];

				if (matchStream.matchText("sampleRate"))
				{
					matchStream.matchWhitespace();
					if (!matchStream.matchChar('='))
						throw string("Syntax Error: Expected '='");
					matchStream.matchWhitespace();
					if (!matchStream.matchUInt(&firSpec.m_SampleFreq))
						throw string("Syntax Error: Expected Unsigned-Integer sample rate");
					matchStream.matchWhitespace();
					if (!matchStream.matchChar(';'))
						throw string("Syntax Error: Expected ';'");
				}
				else if (matchStream.matchText("coeff"))
				{
					firSpec.m_vCoeff.clear();
					matchStream.matchWhitespace();
					if (!matchStream.matchChar('='))
						throw string("Syntax Error: Expected '='");
					matchStream.matchWhitespace();
					if (!matchStream.matchChar('['))
						throw string("Syntax Error: Expected '['");
					matchStream.matchWhitespace();
					while (!matchStream.matchChar(']'))
					{
						firSpec.m_vCoeff.push_back(0);
						if (!matchStream.matchFloatingPointNumber(0, &firSpec.m_vCoeff.back()))
							throw string("Syntax Error: Expected floating point number");
						matchStream.matchWhitespace();
						matchStream.matchChar(',');
						matchStream.matchWhitespace();
					}
					if (!matchStream.matchChar(';'))
						throw string("Syntax Error: Expected ';'");
				}
				else
				{
					throw string("Unrecognized field '") + matchStream.getString() + "'";
				}

				matchStream.matchWhitespace();
				if (!matchStream.atEnd() && !matchStream.matchChar('#'))
				{
					throw string("Unexpected tokens '") + matchStream.getString() + "'";
				}
			}
			else
			{
				throw string("Unrecognized command: '") + matchStream.getString() + "'";
			}
		}
	}
	catch (const string& str)
	{
		throw string("Syntax Error on line ") + toString(lineNum) + ": " + str;
	}
}

void FirEngineSpec::generateHtmlReport(ostream& stream) const
{
	stream << "<h2>FirEngine Specification</h2>\n";

	stream << "<script type=\"text/javascript\">\n";
	stream << "window.onload = function () {\n";
	for (unsigned firIdx = 0; firIdx < m_vFirSpec.size(); ++firIdx)
	{
		const FirSpec& firSpec = m_vFirSpec[firIdx];
		stream << "	var chart" << firIdx << " = new CanvasJS.Chart(\"fir" << firIdx << "Chart\",\n";
		stream << "	{\n";
		stream << "		animationEnabled: true,\n";
		stream << "		title:{\n";
		stream << "			text: \"Fir Coefficients\"\n";
		stream << "		},\n";
		stream << "		data: [\n";
		stream << "		{\n";
		stream << "			type: \"spline\", //change type to bar, line, area, pie, etc\n";
		stream << "			showInLegend: true,\n";        
		stream << "			dataPoints: [\n";
		for (unsigned i = 0; i < firSpec.m_vCoeff.size(); ++i)
		{
			double coeff = firSpec.m_vCoeff[i];
			if (i > 0)
				stream << ",\n";
			stream << "				{ x: " << i << ", y: " << coeff << " }";
		}
		stream << "\n";
		stream << "			]\n";
		stream << "			}\n";
		stream << "		],\n";
		stream << "		legend: {\n";
		stream << "			cursor: \"pointer\",\n";
		stream << "			itemclick: function (e) {\n";
		stream << "				if (typeof(e.dataSeries.visible) === \"undefined\" || e.dataSeries.visible) {\n";
		stream << "					e.dataSeries.visible = false;\n";
		stream << "				} else {\n";
		stream << "					e.dataSeries.visible = true;\n";
		stream << "			}\n";
		stream << "			chart" << firIdx << ".render();\n";
		stream << "			}\n";
		stream << "		}\n";
		stream << "	});\n";
		stream << "\n";
		stream << "	chart" << firIdx << ".render();\n";
	}
	stream << "}\n";
	stream << "</script>\n";

		
	for (unsigned firIdx = 0; firIdx < m_vFirSpec.size(); ++firIdx)
	{
		const FirSpec& firSpec = m_vFirSpec[firIdx];

		stream << "<table class=\"t1\">\n";
		stream << "<tr><th>Fir#</th><td>" << firIdx << "</td></tr>\n";
		stream << "<tr><th>SampleFrequency</th><td>" << firSpec.m_SampleFreq << "</td></tr>\n";
		stream << "<tr><th>NumCoefficients</th><td>" << firSpec.m_vCoeff.size() << "</td></tr>\n";
		stream << "</table>\n\n";

		stream << "<div id=\"fir" << firIdx << "Chart\" style=\"height: 300px; width: 80%;\"></div>\n";
		stream << "\n\n";
	}
}
