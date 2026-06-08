/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/09 17:23:56 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/08 18:47:57 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include "LocationConfig.hpp"

struct ServerConfig {
	std::string					server_name;
	int							listen;
	std::string					host;
	std::string					root;
	std::string					index;
	size_t						client_max_body_size;
	std::map<int, std::string>	error_page;
	std::vector<LocationConfig>	locations;
};

#endif
