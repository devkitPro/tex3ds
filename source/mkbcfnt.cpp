/*------------------------------------------------------------------------------
 * Copyright (c) 2019
 *     Michael Theall (mtheall)
 *     piepie62
 *
 * This file is part of tex3ds.
 *
 * tex3ds is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tex3ds is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tex3ds.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file mkbcfnt.cpp
 *  @brief mkbcfnt program entry point
 */
#include "bcfnt.h"
#include "future.h"

#include <getopt.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
/** @brief Print version information */
void print_version ()
{
	std::printf ("mkbcfnt v1.0.1\n"
	             "Copyright (c) 2019\n"
	             "    Michael Theall (mtheall)\n"
	             "    piepie62\n\n"

	             "mkbcfnt is free software: you can redistribute it and/or modify\n"
	             "it under the terms of the GNU General Public License as published by\n"
	             "the Free Software Foundation, either version 3 of the License, or\n"
	             "(at your option) any later version.\n\n"

	             "mkbcfnt is distributed in the hope that it will be useful,\n"
	             "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	             "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	             "GNU General Public License for more details.\n\n"

	             "You should have received a copy of the GNU General Public License\n"
	             "along with mkbcfnt.  If not, see <http://www.gnu.org/licenses/>.\n");
}

/** @brief Print usage information
 *  @param[in] prog Program invocation
 */
void print_usage (const char *prog)
{
	std::printf ("Usage: %s [OPTIONS...] <input>\n", prog);

	std::printf ("  Options:\n"
	             "    -h, --help                   Show this help message\n"
	             "    -o, --output <output>        Output file\n"
	             "    -s, --size <size>            Set font size in points\n"
	             "    -v, --version                Show version and copyright information\n"
	             "    <input>                      Input file\n\n");
}

/** @brief Program long options */
const struct option long_options[] = {
    /* clang-format off */
	{ "help",     no_argument,       nullptr, 'h', },
	{ "output",   required_argument, nullptr, 'o', },
	{ "size",     required_argument, nullptr, 's', },
	{ "version",  no_argument,       nullptr, 'v', },
	{ nullptr,    no_argument,       nullptr,   0, },
    /* clang-format on */
};
}

/** @brief Program entry point
 *  @param[in] argc Number of command-line arguments
 *  @param[in] argv Command-line arguments
 *  @retval EXIT_SUCCESS
 *  @retval EXIT_FAILURE
 */
int main (int argc, char *argv[])
{
	const char *prog = argv[0];

	std::setvbuf (stdout, nullptr, _IOLBF, 0);
	std::setvbuf (stderr, nullptr, _IOLBF, 0);

	std::string output_path;
	int size = 22;

	// parse options
	int c;
	while ((c = ::getopt_long (argc, argv, "ho:v", long_options, nullptr)) != -1)
	{
		switch (c)
		{
		case 'h':
			// show help
			print_usage (prog);
			return EXIT_SUCCESS;

		case 'o':
			// set output path option
			output_path = optarg;
			break;

		case 's':
			// set font size
			size = atoi (optarg);
			if (!size)
			{
				size = 22;
			}
			break;

		case 'v':
			// print version
			print_version ();
			return EXIT_SUCCESS;

		default:
			print_usage (prog);
			return EXIT_FAILURE;
		}
	}

	if (output_path.empty ())
	{
		std::fprintf (stderr, "No output file provided\n");
		return EXIT_FAILURE;
	}

	if (optind >= argc)
	{
		std::fprintf (stderr, "No input file provided\n");
		return EXIT_FAILURE;
	}

	if (optind != argc - 1)
	{
		std::fprintf (stderr, "Too many arguments provided\n");
		return EXIT_FAILURE;
	}

	std::string input_path = argv[optind];

	auto bcfnt = future::make_unique<bcfnt::BCFNT> ();

	if (!bcfnt->addFont (input_path, size))
		return EXIT_FAILURE;

	if (!bcfnt->serialize (output_path))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
	;
}
