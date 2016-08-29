#include "cvcf_reader.hpp"

#include <assert.h>
#include <algorithm>


namespace vc
{
  namespace cvcf
  {
    //================================================================//
    const marker::const_iterator::value_type marker::const_iterator::const_is_missing = allele_status::is_missing;
    const marker::const_iterator::value_type marker::const_iterator::const_has_ref = allele_status::has_ref;
    const marker::const_iterator::value_type marker::const_iterator::const_has_alt = allele_status::has_alt;

    const allele_status const_is_missing = allele_status::is_missing;
    const allele_status const_has_ref = allele_status::has_ref;
    const allele_status const_has_alt = allele_status::has_alt;

    const allele_status& marker::operator[](std::uint64_t i) const
    {
      auto end = non_zero_haplotypes_.end();
      sparse_vector_allele val;
      val.offset = i;
      auto it = std::lower_bound(non_zero_haplotypes_.begin(), end, val, [](const sparse_vector_allele& a, const sparse_vector_allele& b) { return a.offset < b.offset; });
      if (it == end || it->offset != i)
        return const_has_ref;
      else
        return it->status;
    }

    const allele_status& marker::at(std::uint64_t i) const
    {
      if (i >= non_zero_haplotypes_.size())
        throw std::out_of_range("index out of range");
      return (*this)[i];
    }

    marker::non_ref_iterator marker::non_ref_begin() const
    {
      return non_zero_haplotypes_.begin();
    }

    marker::non_ref_iterator marker::non_ref_end() const
    {
      return non_zero_haplotypes_.end();
    }

    marker::const_iterator marker::begin() const
    {
      return const_iterator(0, non_zero_haplotypes_.begin(), non_zero_haplotypes_.end());
    }

    marker::const_iterator marker::end() const
    {
      return const_iterator(haplotype_count_, non_zero_haplotypes_.begin(), non_zero_haplotypes_.end());
    }

    double marker::calculate_allele_frequency() const
    {
      std::uint64_t allele_cnt = 0;
      std::uint64_t total_haplotypes = haplotype_count_;
      for (auto it = non_zero_haplotypes_.begin(); it != non_zero_haplotypes_.end(); ++it)
      {
        if (it->status == allele_status::is_missing)
          --total_haplotypes;
        else // has alt
          ++allele_cnt;
      }

      return static_cast<double>(allele_cnt) / static_cast<double>(total_haplotypes);
    }

    void marker::read(marker& destination, std::uint64_t haplotype_count, std::istream& is)
    {
      destination.haplotype_count_ = haplotype_count;
      std::istreambuf_iterator<char> in_it(is);
      std::istreambuf_iterator<char> end_it;

      if (varint_decode(in_it, end_it, destination.position_) != end_it)
      {
        ++in_it;
        std::uint64_t sz;
        if (varint_decode(in_it, end_it, sz) != end_it)
        {
          ++in_it;
          destination.ref_.resize(sz);
          if (destination.ref_.size())
            is.read(&destination.ref_[0], destination.ref_.size());

          if (varint_decode(in_it, end_it, sz) != end_it)
          {
            ++in_it;
            destination.alt_.resize(sz);
            if (destination.alt_.size())
              is.read(&destination.alt_[0], destination.alt_.size());

            std::uint8_t rle = 0;
            one_bit_prefixed_varint::decode(in_it, end_it, rle, sz);
            if (rle)
            {
              std::vector<std::pair<sparse_vector_allele, std::uint64_t>> rle_non_zero_haplotypes(sz);
              std::uint64_t total_repeat_bytes = 0;
              for (auto it = rle_non_zero_haplotypes.begin(); it != rle_non_zero_haplotypes.end() && in_it != end_it; ++it)
              {
                std::uint8_t allele_repeat_prefix;
                two_bit_prefixed_varint::decode(++in_it, end_it, allele_repeat_prefix, it->first.offset);
                it->first.status = (allele_repeat_prefix & 0x80 ? allele_status::has_alt : allele_status::is_missing);

                if (allele_repeat_prefix & 0x40 && in_it != end_it)
                {
                  // Read repeat byte.
                  varint_decode(++in_it, end_it, it->second);
                  total_repeat_bytes += it->second;
                }
                else
                {
                  it->second = 0;
                }
              }

              std::uint64_t total_offset = 0;
              destination.non_zero_haplotypes_.clear();
              destination.non_zero_haplotypes_.reserve(sz + total_repeat_bytes);
              for (auto it = rle_non_zero_haplotypes.begin(); it != rle_non_zero_haplotypes.end(); ++it,++total_offset)
              {
                total_offset += it->first.offset;
                destination.non_zero_haplotypes_.emplace_back(it->first.status, total_offset);
                while (it->second)
                {
                  total_offset += (it->first.offset + 1);
                  destination.non_zero_haplotypes_.emplace_back(it->first.status, total_offset);
                  --(it->second);
                }
              }
            }
            else
            {
              destination.non_zero_haplotypes_.resize(sz);

              std::uint64_t total_offset = 0;
              for (auto it = destination.non_zero_haplotypes_.begin(); it != destination.non_zero_haplotypes_.end() && in_it != end_it; ++it,++total_offset)
              {
                std::uint8_t allele;
                std::uint64_t offset;
                one_bit_prefixed_varint::decode(++in_it, end_it, allele, offset);
                total_offset += offset;
                it->offset = total_offset;
                it->status = (allele ? allele_status::has_alt : allele_status::is_missing);
              }
            }
          }
        }
      }

      is.get();
    }

    std::size_t marker::calculate_serialized_gt_size() const
    {
      std::size_t ret = 0;

      std::uint64_t last_pos = 0;
      for (auto it = non_zero_haplotypes_.begin(); it != non_zero_haplotypes_.end(); ++it)
      {
        std::uint64_t offset = it->offset - last_pos;
        last_pos = it->offset + 1;
        ret += one_bit_prefixed_varint::encoded_byte_width(offset);
      }

      return ret;
    }

    std::size_t marker::calculate_rle_serialized_gt_size() const
    {
      std::size_t ret = 0;

      std::uint64_t last_pos = 0;
      for (auto it = non_zero_haplotypes_.begin(); it != non_zero_haplotypes_.end(); ++it)
      {
        std::uint64_t offset = it->offset - last_pos;
        last_pos = it->offset + 1;

        std::size_t number_of_repeats = 0;
        while (it + 1 != non_zero_haplotypes_.end())
        {
          auto next_it = (it + 1);
          if (offset != next_it->offset - last_pos || it->status != next_it->status)
            break;

          ++number_of_repeats;
          ++it;
          last_pos = it->offset + 1;
        }

        std::uint8_t allele_repeat_prefix = (it->status == allele_status::has_alt ? std::uint8_t(0x80) : std::uint8_t(0x00));
        if (number_of_repeats)
          allele_repeat_prefix |= 0x40;
        ret += two_bit_prefixed_varint::encoded_byte_width(offset);

        if (number_of_repeats)
          ret += varint_encoded_byte_width(number_of_repeats);
      }

      return ret;
    }

    void marker::write(std::ostream& os, const marker& source)
    {
      std::ostreambuf_iterator<char> os_it(os);
      varint_encode(source.position_, os_it);

      varint_encode(source.ref_.size(), os_it);
      if (source.ref_.size())
        os.write(&source.ref_[0], source.ref_.size());

      varint_encode(source.alt_.size(), os_it);
      if (source.alt_.size())
        os.write(&source.alt_[0], source.alt_.size());


      if (source.calculate_rle_serialized_gt_size() < source.calculate_serialized_gt_size())
      {
        std::uint8_t rle = 0x80;
        one_bit_prefixed_varint::encode(rle, source.non_zero_haplotypes_.size(), os_it);

        std::uint64_t last_pos = 0;
        for (auto it = source.non_zero_haplotypes_.begin(); it != source.non_zero_haplotypes_.end(); ++it)
        {
          std::uint64_t offset = it->offset - last_pos;
          last_pos = it->offset + 1;

          std::size_t number_of_repeats = 0;
          while (it + 1 != source.non_zero_haplotypes_.end())
          {
            auto next_it = (it + 1);
            if (offset != next_it->offset - last_pos || it->status != next_it->status)
              break;

            ++number_of_repeats;
            ++it;
            last_pos = it->offset + 1;
          }

          std::uint8_t allele_repeat_prefix = (it->status == allele_status::has_alt ? std::uint8_t(0x80) : std::uint8_t(0x00));
          if (number_of_repeats)
            allele_repeat_prefix |= 0x40;
          two_bit_prefixed_varint::encode(allele_repeat_prefix, offset, os_it);

          if (number_of_repeats)
            varint_encode(number_of_repeats, os_it);
        }
      }
      else
      {
        std::uint8_t rle = 0x00;
        one_bit_prefixed_varint::encode(rle, source.non_zero_haplotypes_.size(), os_it);

        std::uint64_t last_pos = 0;
        for (auto it = source.non_zero_haplotypes_.begin(); it != source.non_zero_haplotypes_.end(); ++it)
        {
          std::uint64_t offset = it->offset - last_pos;
          last_pos = it->offset + 1;
          std::uint8_t allele = (it->status == allele_status::has_alt ? std::uint8_t(0x80) : std::uint8_t(0x00));
          one_bit_prefixed_varint::encode(allele, offset, os_it);
        }
      }
    }
    //================================================================//

    //================================================================//
    reader::reader(std::istream& input_stream) :
      input_stream_(input_stream)
    {
      std::string version_string(8, '\0');
      input_stream_.read(&version_string[0], version_string.size());

      std::uint64_t sample_size;
      std::istreambuf_iterator<char> in_it(input_stream_);
      std::istreambuf_iterator<char> end;

      if (varint_decode(in_it, end, sample_size) != end)
      {
        ++in_it;
        sample_ids_.reserve(sample_size);

        std::uint64_t id_sz;
        while (sample_size && varint_decode(in_it, end, id_sz) != end)
        {
            ++in_it;
            sample_ids_.emplace_back();
            if (id_sz)
            {
              sample_ids_.back().resize(id_sz);
              input_stream_.read(&sample_ids_.back()[0], id_sz);
            }
            --sample_size;
        }

        if (sample_size == 0)
        {
          std::uint64_t sz;
          if (varint_decode(in_it, end, sz) != end)
          {
            ++in_it;
            if (sz)
            {
              chromosome_.resize(sz);
              input_stream_.read(&chromosome_[0], sz);
            }

            varint_decode(in_it, end, sz);
            assert(sz < 256);
            ploidy_level_ = static_cast<std::uint8_t>(sz);
          }
        }
      }

      input_stream_.get();
    }

    reader& reader::operator>>(marker& destination)
    {
      marker::read(destination, sample_ids_.size() * ploidy_level_, input_stream_);
      return *this;
    }
    //================================================================//
  }
}

